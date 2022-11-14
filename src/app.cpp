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
void app::setup(uint32_t timeout) {
    Serial.begin(115200);
    while (!Serial)
        yield();

    resetSystem();
    mSettings.setup();
    mSettings.getPtr(mConfig);

    mWifi = new ahoywifi(mConfig);

    mSys = new HmSystemType();
    mSys->enableDebug();
    mShouldReboot = false;

    mWifi->setup(timeout, mSettings.getValid());

    mSys->setup(mConfig->nrf.amplifierPower, mConfig->nrf.pinIrq, mConfig->nrf.pinCe, mConfig->nrf.pinCs);
    mSys->addInverters(&mConfig->inst);
    mPayload.setup(mSys);
    mPayload.enableSerialDebug(mConfig->serial.debug);
#ifndef AP_ONLY
    setupMqtt();
    if(mMqttActive)
        mPayload.addListener(std::bind(&MqttType::payloadEventListener, &mMqtt, std::placeholders::_1));
#endif
    setupLed();


    mWebInst = new web(this, mConfig, &mStat, mVersion);
    mWebInst->setup();
    mWebInst->setProtection(strlen(mConfig->sys.adminPwd) != 0);
    DPRINTLN(DBG_INFO, F("Settings valid: ") + String((mSettings.getValid()) ? F("true") : F("false")));

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

    if (ah::checkTicker(&mNtpRefreshTicker, mNtpRefreshInterval)) {
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
        mMqtt.sendMqttDiscoveryConfig(mConfig->mqtt.topic);
    }

    mSys->Radio.loop();

    yield();

    if (ah::checkTicker(&mRxTicker, 5)) {
        bool rxRdy = mSys->Radio.switchRxCh();

        if (!mSys->BufCtrl.empty()) {
            uint8_t len;
            packet_t *p = mSys->BufCtrl.getBack();

            if (mSys->Radio.checkPaketCrc(p->packet, &len, p->rxCh)) {
                // process buffer only on first occurrence
                if (mConfig->serial.debug) {
                    DPRINT(DBG_INFO, "RX " + String(len) + "B Ch" + String(p->rxCh) + " | ");
                    mSys->Radio.dumpBuf(NULL, p->packet, len);
                }

                mStat.frmCnt++;

                if (0 != len)
                    mPayload.add(p, len);
            }
            mSys->BufCtrl.popBack();
        }
        yield();

        if (rxRdy)
            mPayload.process(true, mConfig->nrf.maxRetransPerPyld, &mStat);
    }

    if (mMqttActive)
        mMqtt.loop();

    if (ah::checkTicker(&mTicker, 1000)) {
        if (mUtcTimestamp > 946684800 && mConfig->sun.lat && mConfig->sun.lon && (mUtcTimestamp + mCalculatedTimezoneOffset) / 86400 != (mLatestSunTimestamp + mCalculatedTimezoneOffset) / 86400) {  // update on reboot or midnight
            if (!mLatestSunTimestamp) {                                                                                                                                                           // first call: calculate time zone from longitude to refresh at local midnight
                mCalculatedTimezoneOffset = (int8_t)((mConfig->sun.lon >= 0 ? mConfig->sun.lon + 7.5 : mConfig->sun.lon - 7.5) / 15) * 3600;
            }
            ah::calculateSunriseSunset(mUtcTimestamp, mCalculatedTimezoneOffset, mConfig->sun.lat, mConfig->sun.lon, &mSunrise, &mSunset);
            mLatestSunTimestamp = mUtcTimestamp;
        }

        if (mConfig->serial.showIv) {
            if (++mSerialTicker >= mConfig->serial.interval) {
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
                                    snprintf(topic, 30, "%s/ch%d/%s", iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
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

        if (++mSendTicker >= mConfig->nrf.sendInterval) {
            mSendTicker = 0;

            if (mUtcTimestamp > 946684800 && (!mConfig->sun.disNightCom || !mLatestSunTimestamp || (mUtcTimestamp >= mSunrise && mUtcTimestamp <= mSunset))) {  // Timestamp is set and (inverter communication only during the day if the option is activated and sunrise/sunset is set)
                if (mConfig->serial.debug)
                    DPRINTLN(DBG_DEBUG, F("Free heap: 0x") + String(ESP.getFreeHeap(), HEX));

                if (!mSys->BufCtrl.empty()) {
                    if (mConfig->serial.debug)
                        DPRINTLN(DBG_DEBUG, F("recbuf not empty! #") + String(mSys->BufCtrl.getFill()));
                }

                int8_t maxLoop = MAX_NUM_INVERTERS;
                Inverter<> *iv = mSys->getInverterByPos(mSendLastIvId);
                do {
                    mSendLastIvId = ((MAX_NUM_INVERTERS - 1) == mSendLastIvId) ? 0 : mSendLastIvId + 1;
                    iv = mSys->getInverterByPos(mSendLastIvId);
                } while ((NULL == iv) && ((maxLoop--) > 0));

                if (NULL != iv) {
                    if (!mPayload.isComplete(iv))
                        mPayload.process(false, mConfig->nrf.maxRetransPerPyld, &mStat);

                    if (!mPayload.isComplete(iv)) {
                        if (0 == mPayload.getMaxPacketId(iv))
                            mStat.rxFailNoAnser++;
                        else
                            mStat.rxFail++;

                        iv->setQueuedCmdFinished();  // command failed
                        if (mConfig->serial.debug)
                            DPRINTLN(DBG_INFO, F("enqueued cmd failed/timeout"));
                        if (mConfig->serial.debug) {
                            DPRINT(DBG_INFO, F("(#") + String(iv->id) + ") ");
                            DPRINTLN(DBG_INFO, F("no Payload received! (retransmits: ") + String(mPayload.getRetransmits(iv)) + ")");
                        }
                    }

                    mPayload.reset(iv, mUtcTimestamp);
                    mPayload.request(iv);

                    yield();
                    if (mConfig->serial.debug) {
                        DPRINTLN(DBG_DEBUG, F("app:loop WiFi WiFi.status ") + String(WiFi.status()));
                        DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") Requesting Inv SN ") + String(iv->config->serial.u64, HEX));
                    }

                    if (iv->devControlRequest) {
                        if (mConfig->serial.debug)
                            DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") Devcontrol request ") + String(iv->devControlCmd) + F(" power limit ") + String(iv->powerLimit[0]));
                        mSys->Radio.sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit);
                        mPayload.setTxCmd(iv, iv->devControlCmd);
                        iv->clearCmdQueue();
                        iv->enqueCommand<InfoCommand>(SystemConfigPara);
                    } else {
                        uint8_t cmd = iv->getQueuedCmd();
                        DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") sendTimePacket"));
                        mSys->Radio.sendTimePacket(iv->radioId.u64, cmd, mPayload.getTs(iv), iv->alarmMesIndex);
                        mPayload.setTxCmd(iv, cmd);
                        mRxTicker = 0;
                    }
                }
            } else if (mConfig->serial.debug)
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
    mSerialTicker = 0xffff;
    mMqttActive = false;

    mTicker = 0;
    mRxTicker = 0;

    mSendLastIvId = 0;

    mShowRebootRequest = false;

    memset(&mStat, 0, sizeof(statistics_t));
}

//-----------------------------------------------------------------------------
void app::setupMqtt(void) {
    if (mConfig->mqtt.broker[0] > 0)
        mMqttActive = true;

    if(mMqttActive)
        mMqtt.setup(&mConfig->mqtt, mConfig->sys.deviceName, mVersion, mSys, &mUtcTimestamp);
}

//-----------------------------------------------------------------------------
void app::setupLed(void) {
    /** LED connection diagram
     *          \\
     * PIN ---- |<----- 3.3V
     *
     * */
    if(mConfig->led.led0 != 0xff) {
        pinMode(mConfig->led.led0, OUTPUT);
        digitalWrite(mConfig->led.led0, HIGH); // LED off
    }
    if(mConfig->led.led1 != 0xff) {
        pinMode(mConfig->led.led1, OUTPUT);
        digitalWrite(mConfig->led.led1, HIGH); // LED off
    }
}

//-----------------------------------------------------------------------------
void app::updateLed(void) {
    if(mConfig->led.led0 != 0xff) {
        Inverter<> *iv = mSys->getInverterByPos(0);
        if (NULL != iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            if(iv->isProducing(mUtcTimestamp, rec))
                digitalWrite(mConfig->led.led0, LOW); // LED on
            else
                digitalWrite(mConfig->led.led0, HIGH); // LED off
        }
    }
}
