//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
#undef F
#define F(sl) (sl)
#endif

#include "app.h"
#include <ArduinoJson.h>
#include "utils/sun.h"

//-----------------------------------------------------------------------------
app::app() {
    Serial.begin(115200);
    DPRINTLN(DBG_VERBOSE, F("app::app"));
    mEep = new eep();

    resetSystem();
    loadDefaultConfig();

    mWifi = new ahoywifi(this, &mSysConfig, &mConfig);
    mSys = new HmSystemType();
    mSys->enableDebug();
    mShouldReboot = false;
}

//-----------------------------------------------------------------------------
void app::setup(uint32_t timeout) {
    DPRINTLN(DBG_VERBOSE, F("app::setup"));

    mWifiSettingsValid = checkEEpCrc(ADDR_START, ADDR_WIFI_CRC, ADDR_WIFI_CRC);
    mSettingsValid = checkEEpCrc(ADDR_START_SETTINGS, ((ADDR_NEXT) - (ADDR_START_SETTINGS)), ADDR_SETTINGS_CRC);
    loadEEpconfig();

    mWifi->setup(timeout, mWifiSettingsValid);

    mSys->setup(mConfig.amplifierPower, mConfig.pinIrq, mConfig.pinCe, mConfig.pinCs);
#ifndef AP_ONLY
    setupMqtt();
#endif
    setupLed();

    mWebInst = new web(this, &mSysConfig, &mConfig, &mStat, mVersion);
    mWebInst->setup();
    mWebInst->setProtection(strlen(mConfig.password) != 0);
    DPRINTLN(DBG_INFO, F("Settings valid: ") + String((mSettingsValid) ? F("true") : F("false")));
    DPRINTLN(DBG_INFO, F("EEprom storage size: 0x") + String(ADDR_SETTINGS_CRC, HEX));
}

//-----------------------------------------------------------------------------
void app::loop(void) {
    DPRINTLN(DBG_VERBOSE, F("app::loop"));

    bool apActive = mWifi->loop();
    mWebInst->loop();

    if (millis() - mPrevMillis >= 1000) {
        mPrevMillis += 1000;
        mUptimeSecs++;
        if (0 != mUtcTimestamp)
            mUtcTimestamp++;

        mWebInst->tickSecond();

        if (mShouldReboot) {
            DPRINTLN(DBG_INFO, F("Rebooting..."));
            ESP.restart();
        }
    }

    if (checkTicker(&mNtpRefreshTicker, mNtpRefreshInterval)) {
        if (!apActive)
            mUpdateNtp = true;
    }

    if (mUpdateNtp) {
        mUpdateNtp = false;
        mUtcTimestamp = mWifi->getNtpTime();
        DPRINTLN(DBG_INFO, F("[NTP]: ") + getDateTimeStr(mUtcTimestamp) + F(" UTC"));
    }

    if (mFlagSendDiscoveryConfig) {
        mFlagSendDiscoveryConfig = false;
        mMqtt.sendMqttDiscoveryConfig(mConfig.mqtt.topic, mMqttInterval);
    }

    mSys->Radio.loop();

    yield();

    if (checkTicker(&mRxTicker, 5)) {
        bool rxRdy = mSys->Radio.switchRxCh();

        if (!mSys->BufCtrl.empty()) {
            uint8_t len;
            packet_t *p = mSys->BufCtrl.getBack();

            if (mSys->Radio.checkPaketCrc(p->packet, &len, p->rxCh)) {
                // process buffer only on first occurrence
                if (mConfig.serialDebug) {
                    DPRINT(DBG_INFO, "RX " + String(len) + "B Ch" + String(p->rxCh) + " | ");
                    mSys->Radio.dumpBuf(NULL, p->packet, len);
                }

                mStat.frmCnt++;

                if (0 != len) {
                    Inverter<> *iv = mSys->findInverter(&p->packet[1]);
                    if ((NULL != iv) && (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES))) {  // response from get information command
                        mPayload[iv->id].txId = p->packet[0];
                        DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                        uint8_t *pid = &p->packet[9];
                        if (*pid == 0x00) {
                            DPRINT(DBG_DEBUG, F("fragment number zero received and ignored"));
                        } else {
                            DPRINTLN(DBG_DEBUG, "PID: 0x" + String(*pid, HEX));
                            if ((*pid & 0x7F) < 5) {
                                memcpy(mPayload[iv->id].data[(*pid & 0x7F) - 1], &p->packet[10], len - 11);
                                mPayload[iv->id].len[(*pid & 0x7F) - 1] = len - 11;
                            }

                            if ((*pid & ALL_FRAMES) == ALL_FRAMES) {
                                // Last packet
                                if ((*pid & 0x7f) > mPayload[iv->id].maxPackId) {
                                    mPayload[iv->id].maxPackId = (*pid & 0x7f);
                                    if (*pid > 0x81)
                                        mLastPacketId = *pid;
                                }
                            }
                        }
                    }
                    if ((NULL != iv) && (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES))) { // response from dev control command
                        DPRINTLN(DBG_DEBUG, F("Response from devcontrol request received"));

                        mPayload[iv->id].txId = p->packet[0];
                        iv->devControlRequest = false;

                        if ((p->packet[12] == ActivePowerContr) && (p->packet[13] == 0x00)) {
                            String msg = (p->packet[10] == 0x00 && p->packet[11] == 0x00) ? "" : "NOT ";
                            DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(" has ") + msg + F("accepted power limit set point ") + String(iv->powerLimit[0]) + F(" with PowerLimitControl ") + String(iv->powerLimit[1]));
                        }
                        iv->devControlCmd = Init;
                    }
                }
            }
            mSys->BufCtrl.popBack();
        }
        yield();

        if (rxRdy) {
            processPayload(true);
        }
    }

    if (mMqttActive)
        mMqtt.loop();

    if (checkTicker(&mTicker, 1000)) {
        if (mUtcTimestamp > 946684800 && mConfig.sunLat && mConfig.sunLon && (mUtcTimestamp + mCalculatedTimezoneOffset) / 86400 != (mLatestSunTimestamp + mCalculatedTimezoneOffset) / 86400) {  // update on reboot or midnight
            if (!mLatestSunTimestamp) {                                                                                                                                                           // first call: calculate time zone from longitude to refresh at local midnight
                mCalculatedTimezoneOffset = (int8_t)((mConfig.sunLon >= 0 ? mConfig.sunLon + 7.5 : mConfig.sunLon - 7.5) / 15) * 3600;
            }
            ah::calculateSunriseSunset(mUtcTimestamp, mCalculatedTimezoneOffset, mConfig.sunLat, mConfig.sunLon, &mSunrise, &mSunset);
            mLatestSunTimestamp = mUtcTimestamp;
        }

        if ((++mMqttTicker >= mMqttInterval) && (mMqttInterval != 0xffff) && mMqttActive) {
            mMqttTicker = 0;
            mMqtt.sendIvData(mUtcTimestamp, mMqttSendList);
        }

        if (mConfig.serialShowIv) {
            if (++mSerialTicker >= mConfig.serialInterval) {
                mSerialTicker = 0;
                char topic[30], val[10];
                for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                    Inverter<> *iv = mSys->getInverterByPos(id);
                    if (NULL != iv) {
                        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                        if (iv->isAvailable(mUtcTimestamp, rec)) {
                            DPRINTLN(DBG_INFO, "Inverter: " + String(id));
                            for (uint8_t i = 0; i < rec->length; i++) {
                                if (0.0f != iv->getValue(i, rec)) {
                                    snprintf(topic, 30, "%s/ch%d/%s", iv->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                                    snprintf(val, 10, "%.3f %s", iv->getValue(i, rec), iv->getUnit(i, rec));
                                    DPRINTLN(DBG_INFO, String(topic) + ": " + String(val));
                                }
                                yield();
                            }
                            DPRINTLN(DBG_INFO, "");
                        }
                    }
                }
            }
        }

        if (++mSendTicker >= mConfig.sendInterval) {
            mSendTicker = 0;

            if (mUtcTimestamp > 946684800 && (!mConfig.sunDisNightCom || !mLatestSunTimestamp || (mUtcTimestamp >= mSunrise && mUtcTimestamp <= mSunset))) {  // Timestamp is set and (inverter communication only during the day if the option is activated and sunrise/sunset is set)
                if (mConfig.serialDebug)
                    DPRINTLN(DBG_DEBUG, F("Free heap: 0x") + String(ESP.getFreeHeap(), HEX));

                if (!mSys->BufCtrl.empty()) {
                    if (mConfig.serialDebug)
                        DPRINTLN(DBG_DEBUG, F("recbuf not empty! #") + String(mSys->BufCtrl.getFill()));
                }

                int8_t maxLoop = MAX_NUM_INVERTERS;
                Inverter<> *iv = mSys->getInverterByPos(mSendLastIvId);
                do {
                    // if(NULL != iv)
                    //     mPayload[iv->id].requested = false;
                    mSendLastIvId = ((MAX_NUM_INVERTERS - 1) == mSendLastIvId) ? 0 : mSendLastIvId + 1;
                    iv = mSys->getInverterByPos(mSendLastIvId);
                } while ((NULL == iv) && ((maxLoop--) > 0));

                if (NULL != iv) {
                    if (!mPayload[iv->id].complete)
                        processPayload(false);

                    if (!mPayload[iv->id].complete) {
                        if (0 == mPayload[iv->id].maxPackId)
                            mStat.rxFailNoAnser++;
                        else
                            mStat.rxFail++;

                        iv->setQueuedCmdFinished();  // command failed
                        if (mConfig.serialDebug)
                            DPRINTLN(DBG_INFO, F("enqueued cmd failed/timeout"));
                        if (mConfig.serialDebug) {
                            DPRINT(DBG_INFO, F("(#") + String(iv->id) + ") ");
                            DPRINTLN(DBG_INFO, F("no Payload received! (retransmits: ") + String(mPayload[iv->id].retransmits) + ")");
                        }
                    }

                    resetPayload(iv);
                    mPayload[iv->id].requested = true;

                    yield();
                    if (mConfig.serialDebug) {
                        DPRINTLN(DBG_DEBUG, F("app:loop WiFi WiFi.status ") + String(WiFi.status()));
                        DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") Requesting Inv SN ") + String(iv->serial.u64, HEX));
                    }

                    if (iv->devControlRequest) {
                        if (mConfig.serialDebug)
                            DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") Devcontrol request ") + String(iv->devControlCmd) + F(" power limit ") + String(iv->powerLimit[0]));
                        mSys->Radio.sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit);
                        mPayload[iv->id].txCmd = iv->devControlCmd;
                        iv->clearCmdQueue();
                        iv->enqueCommand<InfoCommand>(SystemConfigPara);
                    } else {
                        uint8_t cmd = iv->getQueuedCmd();
                        DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") sendTimePacket"));
                        mSys->Radio.sendTimePacket(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex);
                        mPayload[iv->id].txCmd = cmd;
                        mRxTicker = 0;
                    }
                }
            } else if (mConfig.serialDebug)
                DPRINTLN(DBG_WARN, F("Time not set or it is night time, therefore no communication to the inverter!"));
            yield();

            updateLed();
        }
    }
}

//-----------------------------------------------------------------------------
void app::handleIntr(void) {
    DPRINTLN(DBG_VERBOSE, F("app::handleIntr"));
    mSys->Radio.handleIntr();
}

//-----------------------------------------------------------------------------
bool app::buildPayload(uint8_t id) {
    DPRINTLN(DBG_VERBOSE, F("app::buildPayload"));
    uint16_t crc = 0xffff, crcRcv = 0x0000;
    if (mPayload[id].maxPackId > MAX_PAYLOAD_ENTRIES)
        mPayload[id].maxPackId = MAX_PAYLOAD_ENTRIES;

    for (uint8_t i = 0; i < mPayload[id].maxPackId; i++) {
        if (mPayload[id].len[i] > 0) {
            if (i == (mPayload[id].maxPackId - 1)) {
                crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i] - 2, crc);
                crcRcv = (mPayload[id].data[i][mPayload[id].len[i] - 2] << 8) | (mPayload[id].data[i][mPayload[id].len[i] - 1]);
            } else
                crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i], crc);
        }
        yield();
    }

    return (crc == crcRcv) ? true : false;
}

//-----------------------------------------------------------------------------
void app::processPayload(bool retransmit) {
    for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
        Inverter<> *iv = mSys->getInverterByPos(id);
        if (NULL == iv)
            continue; // skip to next inverter

        if ((mPayload[iv->id].txId != (TX_REQ_INFO + ALL_FRAMES)) && (0 != mPayload[iv->id].txId)) {
            // no processing needed if txId is not 0x95
            // DPRINTLN(DBG_INFO, F("processPayload - set complete, txId: ") + String(mPayload[iv->id].txId, HEX));
            mPayload[iv->id].complete = true;
        }

        if (!mPayload[iv->id].complete) {
            if (!buildPayload(iv->id)) { // payload not complete
                if ((mPayload[iv->id].requested) && (retransmit)) {
                    if (iv->devControlCmd == Restart || iv->devControlCmd == CleanState_LockAndAlarm) {
                        // This is required to prevent retransmissions without answer.
                        DPRINTLN(DBG_INFO, F("Prevent retransmit on Restart / CleanState_LockAndAlarm..."));
                        mPayload[iv->id].retransmits = mConfig.maxRetransPerPyld;
                    } else {
                        if (mPayload[iv->id].retransmits < mConfig.maxRetransPerPyld) {
                            mPayload[iv->id].retransmits++;
                            if (mPayload[iv->id].maxPackId != 0) {
                                for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                    if (mPayload[iv->id].len[i] == 0) {
                                        if (mConfig.serialDebug)
                                            DPRINTLN(DBG_WARN, F("while retrieving data: Frame ") + String(i + 1) + F(" missing: Request Retransmit"));
                                        mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                        break;  // only retransmit one frame per loop
                                    }
                                    yield();
                                }
                            } else {
                                if (mConfig.serialDebug)
                                    DPRINTLN(DBG_WARN, F("while retrieving data: last frame missing: Request Retransmit"));
                                if (0x00 != mLastPacketId)
                                    mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, mLastPacketId, true);
                                else {
                                    mPayload[iv->id].txCmd = iv->getQueuedCmd();
                                    DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") sendTimePacket"));
                                    mSys->Radio.sendTimePacket(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex);
                                }
                            }
                            mSys->Radio.switchRxCh(100);
                        }
                    }
                }
            } else {  // payload complete
                DPRINTLN(DBG_INFO, F("procPyld: cmd:  ") + String(mPayload[iv->id].txCmd));
                DPRINTLN(DBG_INFO, F("procPyld: txid: 0x") + String(mPayload[iv->id].txId, HEX));
                DPRINTLN(DBG_DEBUG, F("procPyld: max:  ") + String(mPayload[iv->id].maxPackId));
                record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                mPayload[iv->id].complete = true;

                uint8_t payload[128];
                uint8_t payloadLen = 0;

                memset(payload, 0, 128);

                for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i++) {
                    memcpy(&payload[payloadLen], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                    payloadLen += (mPayload[iv->id].len[i]);
                    yield();
                }
                payloadLen -= 2;

                if (mConfig.serialDebug) {
                    DPRINT(DBG_INFO, F("Payload (") + String(payloadLen) + "): ");
                    mSys->Radio.dumpBuf(NULL, payload, payloadLen);
                }

                if (NULL == rec) {
                    DPRINTLN(DBG_ERROR, F("record is NULL!"));
                } else if ((rec->pyldLen == payloadLen) || (0 == rec->pyldLen)) {
                    if (mPayload[iv->id].txId == (TX_REQ_INFO + 0x80))
                        mStat.rxSuccess++;

                    rec->ts = mPayload[iv->id].ts;
                    for (uint8_t i = 0; i < rec->length; i++) {
                        iv->addValue(i, payload, rec);
                        yield();
                    }
                    iv->doCalculations();

                    mMqttSendList.push(mPayload[iv->id].txCmd);
                } else {
                    DPRINTLN(DBG_ERROR, F("plausibility check failed, expected ") + String(rec->pyldLen) + F(" bytes"));
                    mStat.rxFail++;
                }

                iv->setQueuedCmdFinished();
            }
        }

        yield();

    }

    //  ist MQTT aktiviert und es wurden Daten vom einem oder mehreren WR aufbereitet
    //  dann die den mMqttTicker auf mMqttIntervall -2 setzen, also
    //  MQTT aussenden in 2 sek aktivieren
    if ((mMqttInterval != 0xffff) && (!mMqttSendList.empty())) {
        mMqttTicker = mMqttInterval - 2;
    }
}

//-----------------------------------------------------------------------------
bool app::getWifiApActive(void) {
    return mWifi->getApActive();
}

//-----------------------------------------------------------------------------
void app::scanAvailNetworks(void) {
    mWifi->scanAvailNetworks();
}

//-----------------------------------------------------------------------------
void app::getAvailNetworks(JsonObject obj) {
    mWifi->getAvailNetworks(obj);
}


//-----------------------------------------------------------------------------
void app::resetSystem(void) {
    mUptimeSecs = 0;
    mPrevMillis = 0;
    mUpdateNtp = false;
    mFlagSendDiscoveryConfig = false;

    mNtpRefreshTicker = 0;
    mNtpRefreshInterval = NTP_REFRESH_INTERVAL;  // [ms]

#ifdef AP_ONLY
    mUtcTimestamp = 1;
#else
    mUtcTimestamp = 0;
#endif

    mHeapStatCnt = 0;

    mSendTicker = 0xffff;
    mMqttTicker = 0xffff;
    mMqttInterval = MQTT_INTERVAL;
    mSerialTicker = 0xffff;
    mMqttActive = false;

    mTicker = 0;
    mRxTicker = 0;

    mSendLastIvId = 0;

    mShowRebootRequest = false;

    memset(mPayload, 0, (MAX_NUM_INVERTERS * sizeof(invPayload_t)));
    memset(&mStat, 0, sizeof(statistics_t));
    mLastPacketId = 0x00;
}

//-----------------------------------------------------------------------------
void app::loadDefaultConfig(void) {
    memset(&mSysConfig, 0, sizeof(sysConfig_t));
    memset(&mConfig, 0, sizeof(config_t));
    snprintf(mVersion, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    snprintf(mSysConfig.deviceName, DEVNAME_LEN, "%s", DEF_DEVICE_NAME);

    // wifi
    snprintf(mSysConfig.stationSsid, SSID_LEN, "%s", FB_WIFI_SSID);
    snprintf(mSysConfig.stationPwd, PWD_LEN, "%s", FB_WIFI_PWD);

    // password
    snprintf(mConfig.password, PWD_LEN, "%s", GUI_DEF_PASSWORD);

    // nrf24
    mConfig.sendInterval = SEND_INTERVAL;
    mConfig.maxRetransPerPyld = DEF_MAX_RETRANS_PER_PYLD;
    mConfig.pinCs = DEF_CS_PIN;
    mConfig.pinCe = DEF_CE_PIN;
    mConfig.pinIrq = DEF_IRQ_PIN;
    mConfig.amplifierPower = DEF_AMPLIFIERPOWER & 0x03;

    // status LED
    mConfig.led.led0 = DEF_LED0_PIN;
    mConfig.led.led1 = DEF_LED1_PIN;

    // ntp
    snprintf(mConfig.ntpAddr, NTP_ADDR_LEN, "%s", DEF_NTP_SERVER_NAME);
    mConfig.ntpPort = DEF_NTP_PORT;

    // Latitude + Longitude
    mConfig.sunLat = 0.0;
    mConfig.sunLon = 0.0;
    mConfig.sunDisNightCom = false;

    // mqtt
    snprintf(mConfig.mqtt.broker, MQTT_ADDR_LEN, "%s", DEF_MQTT_BROKER);
    mConfig.mqtt.port = DEF_MQTT_PORT;
    snprintf(mConfig.mqtt.user, MQTT_USER_LEN, "%s", DEF_MQTT_USER);
    snprintf(mConfig.mqtt.pwd, MQTT_PWD_LEN, "%s", DEF_MQTT_PWD);
    snprintf(mConfig.mqtt.topic, MQTT_TOPIC_LEN, "%s", DEF_MQTT_TOPIC);

    // serial
    mConfig.serialInterval = SERIAL_INTERVAL;
    mConfig.serialShowIv = false;
    mConfig.serialDebug = false;

    // Disclaimer
    mConfig.disclaimer = false;
}

//-----------------------------------------------------------------------------
void app::loadEEpconfig(void) {
    DPRINTLN(DBG_INFO, F("loadEEpconfig"));

    if (mWifiSettingsValid)
        mEep->read(ADDR_CFG_SYS, (uint8_t *)&mSysConfig, CFG_SYS_LEN);
    if (mSettingsValid) {
        mEep->read(ADDR_CFG, (uint8_t *)&mConfig, CFG_LEN);

        mSendTicker = mConfig.sendInterval;
        mSerialTicker = 0;

        // inverter
        uint64_t invSerial;
        char name[MAX_NAME_LENGTH + 1] = {0};
        uint16_t modPwr[4];
        Inverter<> *iv;
        for (uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
            mEep->read(ADDR_INV_ADDR + (i * 8), &invSerial);
            mEep->read(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), name, MAX_NAME_LENGTH);
            mEep->read(ADDR_INV_CH_PWR + (i * 2 * 4), modPwr, 4);
            if (0ULL != invSerial) {
                iv = mSys->addInverter(name, invSerial, modPwr);
                if (NULL != iv) {  // will run once on every dtu boot
                    for (uint8_t j = 0; j < 4; j++) {
                        mEep->read(ADDR_INV_CH_NAME + (i * 4 * MAX_NAME_LENGTH) + j * MAX_NAME_LENGTH, iv->chName[j], MAX_NAME_LENGTH);
                    }
                }

                // TODO: the original mqttinterval value is not needed any more
                mMqttInterval += mConfig.sendInterval;
            }
        }

        for (uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
            iv = mSys->getInverterByPos(i, false);
            if (NULL != iv)
                resetPayload(iv);
        }
    }
}

//-----------------------------------------------------------------------------
void app::saveValues(void) {
    DPRINTLN(DBG_VERBOSE, F("app::saveValues"));

    mEep->write(ADDR_CFG_SYS, (uint8_t *)&mSysConfig, CFG_SYS_LEN);
    mEep->write(ADDR_CFG, (uint8_t *)&mConfig, CFG_LEN);
    Inverter<> *iv;
    for (uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
        iv = mSys->getInverterByPos(i, false);
        mEep->write(ADDR_INV_ADDR + (i * 8), iv->serial.u64);
        mEep->write(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), iv->name, MAX_NAME_LENGTH);
        // max channel power / name
        for (uint8_t j = 0; j < 4; j++) {
            mEep->write(ADDR_INV_CH_PWR + (i * 2 * 4) + (j * 2), iv->chMaxPwr[j]);
            mEep->write(ADDR_INV_CH_NAME + (i * 4 * MAX_NAME_LENGTH) + j * MAX_NAME_LENGTH, iv->chName[j], MAX_NAME_LENGTH);
        }
    }

    updateCrc();

    // update sun
    mLatestSunTimestamp = 0;
}

//-----------------------------------------------------------------------------
void app::setupMqtt(void) {
    if (mSettingsValid) {
        if (mConfig.mqtt.broker[0] > 0) {
            mMqttActive = true;
            if (mMqttInterval < MIN_MQTT_INTERVAL) mMqttInterval = MIN_MQTT_INTERVAL;
        } else
            mMqttInterval = 0xffff;

        mMqttTicker = 0;
        if(mMqttActive)
            mMqtt.setup(&mConfig.mqtt, mSysConfig.deviceName, mSys);

        if (mMqttActive) {
            mMqtt.sendMsg("version", mVersion);
            if (mMqtt.isConnected()) {
                mMqtt.sendMsg("device", mSysConfig.deviceName);
                mMqtt.sendMsg("uptime", "0");
            }
        }
    }
}

//-----------------------------------------------------------------------------
void app::setupLed(void) {
    /** LED connection diagram
     *          \\
     * PIN ---- |<----- 3.3V
     *
     * */
    if(mConfig.led.led0 != 0xff) {
        pinMode(mConfig.led.led0, OUTPUT);
        digitalWrite(mConfig.led.led0, HIGH); // LED off
    }
    if(mConfig.led.led1 != 0xff) {
        pinMode(mConfig.led.led1, OUTPUT);
        digitalWrite(mConfig.led.led1, HIGH); // LED off
    }
}

//-----------------------------------------------------------------------------
void app::updateLed(void) {
    if(mConfig.led.led0 != 0xff) {
        Inverter<> *iv = mSys->getInverterByPos(0);
        if (NULL != iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            if(iv->isProducing(mUtcTimestamp, rec))
                digitalWrite(mConfig.led.led0, LOW); // LED on
            else
                digitalWrite(mConfig.led.led0, HIGH); // LED off
        }
    }
}

//-----------------------------------------------------------------------------
void app::resetPayload(Inverter<> *iv) {
    DPRINTLN(DBG_INFO, "resetPayload: id: " + String(iv->id));
    memset(mPayload[iv->id].len, 0, MAX_PAYLOAD_ENTRIES);
    mPayload[iv->id].txCmd = 0;
    mPayload[iv->id].retransmits = 0;
    mPayload[iv->id].maxPackId = 0;
    mPayload[iv->id].complete = false;
    mPayload[iv->id].requested = false;
    mPayload[iv->id].ts = mUtcTimestamp;
}
