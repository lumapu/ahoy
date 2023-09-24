//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#include <ArduinoJson.h>

#include "app.h"

#include "utils/sun.h"


//-----------------------------------------------------------------------------
app::app() : ah::Scheduler {} {}


//-----------------------------------------------------------------------------
void app::setup() {
    Serial.begin(115200);
    while (!Serial)
        yield();


    resetSystem();

    mSettings.setup();
    mSettings.getPtr(mConfig);
    ah::Scheduler::setup(mConfig->inst.startWithoutTime);
    DPRINT(DBG_INFO, F("Settings valid: "));
    DSERIAL.flush();
    if (mSettings.getValid())
        DBGPRINTLN(F("true"));
    else
        DBGPRINTLN(F("false"));

    if(mConfig->nrf.enabled) {
        mNrfRadio.setup(&mNrfStat, mConfig->nrf.amplifierPower, mConfig->nrf.pinIrq, mConfig->nrf.pinCe, mConfig->nrf.pinCs, mConfig->nrf.pinSclk, mConfig->nrf.pinMosi, mConfig->nrf.pinMiso);
        mNrfRadio.enableDebug();
    }
    #if defined(ESP32)
    if(mConfig->cmt.enabled) {
        mCmtRadio.setup(&mCmtStat, mConfig->cmt.pinSclk, mConfig->cmt.pinSdio, mConfig->cmt.pinCsb, mConfig->cmt.pinFcsb, false);
        mCmtRadio.enableDebug();
    }
    #endif
    #ifdef ETHERNET
        delay(1000);
        DPRINT(DBG_INFO, F("mEth setup..."));
        DSERIAL.flush();
        mEth.setup(mConfig, &mTimestamp, [this](bool gotIp) { this->onNetwork(gotIp); }, [this](bool gotTime) { this->onNtpUpdate(gotTime); });
        DBGPRINTLN(F("done..."));
        DSERIAL.flush();
    #endif // ETHERNET

    #if !defined(ETHERNET)
        mWifi.setup(mConfig, &mTimestamp, std::bind(&app::onNetwork, this, std::placeholders::_1));
        #if !defined(AP_ONLY)
            everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi), "wifiL");
        #endif
    #endif /* defined(ETHERNET) */

    mSys.setup(&mTimestamp);
    mSys.addInverters(&mConfig->inst);
    if (mConfig->nrf.enabled) {
        mPayload.setup(this, &mSys, &mNrfRadio, &mNrfStat, mConfig->nrf.maxRetransPerPyld, &mTimestamp);
        mPayload.enableSerialDebug(mConfig->serial.debug);
        mPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1, std::placeholders::_2));

        mMiPayload.setup(this, &mSys, &mNrfRadio, &mNrfStat, mConfig->nrf.maxRetransPerPyld, &mTimestamp);
        mMiPayload.enableSerialDebug(mConfig->serial.debug);
        mMiPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1, std::placeholders::_2));
    }

    #if defined(ESP32)
        mHmsPayload.setup(this, &mSys, &mCmtRadio, &mCmtStat, 5, &mTimestamp);
        mHmsPayload.enableSerialDebug(mConfig->serial.debug);
        mHmsPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1, std::placeholders::_2));
    #endif

    if(mConfig->nrf.enabled) {
        if (!mNrfRadio.isChipConnected())
            DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));
    }

    // when WiFi is in client mode, then enable mqtt broker
    #if !defined(AP_ONLY)
    mMqttEnabled = (mConfig->mqtt.broker[0] > 0);
    if (mMqttEnabled) {
        mMqtt.setup(&mConfig->mqtt, mConfig->sys.deviceName, mVersion, &mSys, &mTimestamp, &mUptime);
        mMqtt.setSubscriptionCb(std::bind(&app::mqttSubRxCb, this, std::placeholders::_1));
        mPayload.addAlarmListener([this](Inverter<> *iv) { mMqtt.alarmEvent(iv); });
        mMiPayload.addAlarmListener([this](Inverter<> *iv) { mMqtt.alarmEvent(iv); });
        #if defined(ESP32)
            mHmsPayload.addAlarmListener([this](Inverter<> *iv) { mMqtt.alarmEvent(iv); });
        #endif
    }
    #endif
    setupLed();

    mWeb.setup(this, &mSys, mConfig);
    mWeb.setProtection(strlen(mConfig->sys.adminPwd) != 0);

    mApi.setup(this, &mSys, mWeb.getWebSrvPtr(), mConfig);

    // Plugins
    if (mConfig->plugin.display.type != 0)
        mDisplay.setup(this, &mConfig->plugin.display, &mSys, &mNrfRadio, &mTimestamp);

    mPubSerial.setup(mConfig, &mSys, &mTimestamp);

    // ZeroExport
    if (mConfig->plugin.zexport.enabled) {
        mzExport.setup(&mConfig->plugin.zexport, &mSys, mConfig);
    }

    #if !defined(ETHERNET)
    //mImprov.setup(this, mConfig->sys.deviceName, mVersion);
    #endif

    regularTickers();
}

//-----------------------------------------------------------------------------
void app::loop(void) {
    ah::Scheduler::loop();

    if (mNrfRadio.loop() && mConfig->nrf.enabled) {
        while (!mNrfRadio.mBufCtrl.empty()) {
            packet_t *p = &mNrfRadio.mBufCtrl.front();

            if (mConfig->serial.debug) {
                DPRINT(DBG_INFO, F("RX "));
                DBGPRINT(String(p->len));
                DBGPRINT(F(" CH"));
                DBGPRINT(String(p->ch));
                DBGPRINT(F(", "));
                DBGPRINT(String(p->rssi));
                DBGPRINT(F("dBm | "));
                ah::dumpBuf(p->packet, p->len);
            }
            mNrfStat.frmCnt++;

            Inverter<> *iv = mSys.findInverter(&p->packet[1]);
            if (NULL != iv) {
                if (IV_HM == iv->ivGen)
                    mPayload.add(iv, p);
                else
                    mMiPayload.add(iv, p);
            }
            mNrfRadio.mBufCtrl.pop();
            yield();
        }
        mPayload.process(true);
        mMiPayload.process(true);
    }
    #if defined(ESP32)
    if (mCmtRadio.loop() && mConfig->cmt.enabled) {
        while (!mCmtRadio.mBufCtrl.empty()) {
            hmsPacket_t *p = &mCmtRadio.mBufCtrl.front();
            if (mConfig->serial.debug) {
                DPRINT(DBG_INFO, F("RX "));
                DBGPRINT(String(p->data[0]));
                DBGPRINT(F(", "));
                DBGPRINT(String(p->rssi));
                DBGPRINT(F("dBm | "));
                ah::dumpBuf(&p->data[1], p->data[0]);
            }
            mCmtStat.frmCnt++;

            Inverter<> *iv = mSys.findInverter(&p->data[2]);
            if(NULL != iv) {
                if((iv->ivGen == IV_HMS) || (iv->ivGen == IV_HMT))
                    mHmsPayload.add(iv, p);
            }
            mCmtRadio.mBufCtrl.pop();
            yield();
        }
        mHmsPayload.process(false); //true
    }
    #endif
    mPayload.loop();
    mMiPayload.loop();
    #if defined(ESP32)
    mHmsPayload.loop();
    #endif

    if (mMqttEnabled && mNetworkConnected)
        mMqtt.loop();
}

//-----------------------------------------------------------------------------
void app::onNetwork(bool gotIp) {
    DPRINTLN(DBG_DEBUG, F("onNetwork"));
    mNetworkConnected = gotIp;
    every(std::bind(&app::tickSend, this), mConfig->nrf.sendInterval, "tSend");
    #if defined(ESP32)
    if(mConfig->cmt.enabled)
        everySec(std::bind(&CmtRadioType::tickSecond, &mCmtRadio), "tsCmt");
    #endif
    mMqttReconnect = true;
    mSunrise = 0;  // needs to be set to 0, to reinstall sunrise and ivComm tickers!
    //once(std::bind(&app::tickNtpUpdate, this), 2, "ntp2");
    tickNtpUpdate();
    #if !defined(ETHERNET)
    if (WIFI_AP == WiFi.getMode()) {
        mMqttEnabled = false;
    }
    everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi), "wifiL");
    #endif /* !defined(ETHERNET) */
}

//-----------------------------------------------------------------------------
void app::regularTickers(void) {
    DPRINTLN(DBG_DEBUG, F("regularTickers"));
    everySec(std::bind(&WebType::tickSecond, &mWeb), "webSc");
    // Plugins
    if (mConfig->plugin.display.type != 0)
        everySec(std::bind(&DisplayType::tickerSecond, &mDisplay), "disp");
    // Plugins
    if (mConfig->plugin.zexport.enabled)
        everySec(std::bind(&ZeroExportType::tickerSecond, &mzExport), "zExport");

    every(std::bind(&PubSerialType::tick, &mPubSerial), mConfig->serial.interval, "uart");
    #if !defined(ETHERNET)
    //everySec([this]() { mImprov.tickSerial(); }, "impro");
    #endif
    // every([this]() { mPayload.simulation();}, 15, "simul");
}

#if defined(ETHERNET)
void app::onNtpUpdate(bool gotTime) {
    uint32_t nxtTrig = 5;  // default: check again in 5 sec
    if (gotTime || mTimestamp != 0) {
        this->updateNtp();
        nxtTrig = gotTime ? 43200 : 60;  // depending on NTP update success check again in 12 h or in 1 min
    }
    once(std::bind(&app::tickNtpUpdate, this), nxtTrig, "ntp");
}
#endif /* defined(ETHERNET) */

//-----------------------------------------------------------------------------
void app::updateNtp(void) {
    if (mMqttReconnect && mMqttEnabled) {
        mMqtt.tickerSecond();
        everySec(std::bind(&PubMqttType::tickerSecond, &mMqtt), "mqttS");
        everyMin(std::bind(&PubMqttType::tickerMinute, &mMqtt), "mqttM");
    }

    // only install schedulers once even if NTP wasn't successful in first loop
    if (mMqttReconnect) {  // @TODO: mMqttReconnect is variable which scope has changed
        if (mConfig->inst.rstValsNotAvail)
            everyMin(std::bind(&app::tickMinute, this), "tMin");

        uint32_t localTime = gTimezone.toLocal(mTimestamp);
        uint32_t midTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86400);  // next midnight local time
        onceAt(std::bind(&app::tickMidnight, this), midTrig, "midNi");

        if (mConfig->sys.schedReboot) {
            uint32_t localTime = gTimezone.toLocal(mTimestamp);
            uint32_t rebootTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86410);  // reboot 10 secs after midnght
            if (rebootTrig <= mTimestamp) { //necessary for times other than midnight to prevent reboot loop
               rebootTrig += 86400;
            }
            onceAt(std::bind(&app::tickReboot, this), rebootTrig, "midRe");
        }
    }

    if ((mSunrise == 0) && (mConfig->sun.lat) && (mConfig->sun.lon)) {
        mCalculatedTimezoneOffset = (int8_t)((mConfig->sun.lon >= 0 ? mConfig->sun.lon + 7.5 : mConfig->sun.lon - 7.5) / 15) * 3600;
        tickCalcSunrise();
    }

    mMqttReconnect = false;
}

//-----------------------------------------------------------------------------
void app::tickNtpUpdate(void) {
    uint32_t nxtTrig = 5;  // default: check again in 5 sec
    #if defined(ETHERNET)
    bool isOK = mEth.updateNtpTime();
    #else
    bool isOK = mWifi.getNtpTime();
    #endif
    if (isOK || mTimestamp != 0) {
        this->updateNtp();

        nxtTrig = isOK ? (mConfig->ntp.interval * 60) : 60;  // depending on NTP update success check again in 12h (depends on setting) or in 1 min

        // immediately start communicating
        if (isOK && mSendFirst) {
            mSendFirst = false;
            once(std::bind(&app::tickSend, this), 2, "senOn");
        }

        mMqttReconnect = false;
    }
    once(std::bind(&app::tickNtpUpdate, this), nxtTrig, "ntp");
}

//-----------------------------------------------------------------------------
void app::tickCalcSunrise(void) {
    if (mSunrise == 0)                                          // on boot/reboot calc sun values for current time
        ah::calculateSunriseSunset(mTimestamp, mCalculatedTimezoneOffset, mConfig->sun.lat, mConfig->sun.lon, &mSunrise, &mSunset);

    if (mTimestamp > (mSunset + mConfig->sun.offsetSec))        // current time is past communication stop, calc sun values for next day
        ah::calculateSunriseSunset(mTimestamp + 86400, mCalculatedTimezoneOffset, mConfig->sun.lat, mConfig->sun.lon, &mSunrise, &mSunset);

    tickIVCommunication();

    uint32_t nxtTrig = mSunset + mConfig->sun.offsetSec + 60;    // set next trigger to communication stop, +60 for safety that it is certain past communication stop
    onceAt(std::bind(&app::tickCalcSunrise, this), nxtTrig, "Sunri");
    if (mMqttEnabled)
        tickSun();
}

//-----------------------------------------------------------------------------
void app::tickIVCommunication(void) {
    mIVCommunicationOn = !mConfig->sun.disNightCom; // if sun.disNightCom is false, communication is always on
    if (!mIVCommunicationOn) {  // inverter communication only during the day
        uint32_t nxtTrig;
        if (mTimestamp < (mSunrise - mConfig->sun.offsetSec)) { // current time is before communication start, set next trigger to communication start
            nxtTrig = mSunrise - mConfig->sun.offsetSec;
        } else {
            if (mTimestamp >= (mSunset + mConfig->sun.offsetSec)) { // current time is past communication stop, nothing to do. Next update will be done at midnight by tickCalcSunrise
                nxtTrig = 0;
            } else { // current time lies within communication start/stop time, set next trigger to communication stop
                mIVCommunicationOn = true;
                nxtTrig = mSunset + mConfig->sun.offsetSec;
            }
        }
        if (nxtTrig != 0)
            onceAt(std::bind(&app::tickIVCommunication, this), nxtTrig, "ivCom");
    }
    tickComm();
}

//-----------------------------------------------------------------------------
void app::tickSun(void) {
    // only used and enabled by MQTT (see setup())
    if (!mMqtt.tickerSun(mSunrise, mSunset, mConfig->sun.offsetSec, mConfig->sun.disNightCom))
        once(std::bind(&app::tickSun, this), 1, "mqSun");  // MQTT not connected, retry
}

//-----------------------------------------------------------------------------
void app::tickComm(void) {
    if ((!mIVCommunicationOn) && (mConfig->inst.rstValsCommStop))
        once(std::bind(&app::tickZeroValues, this), mConfig->nrf.sendInterval, "tZero");

    if (mMqttEnabled) {
        if (!mMqtt.tickerComm(!mIVCommunicationOn))
            once(std::bind(&app::tickComm, this), 5, "mqCom");  // MQTT not connected, retry after 5s
    }
}

//-----------------------------------------------------------------------------
void app::tickZeroValues(void) {
    zeroIvValues(!CHECK_AVAIL, SKIP_YIELD_DAY);
}

//-----------------------------------------------------------------------------
void app::tickMinute(void) {
    // only triggered if 'reset values on no avail is enabled'

    zeroIvValues(CHECK_AVAIL, SKIP_YIELD_DAY);
}

//-----------------------------------------------------------------------------
void app::tickMidnight(void) {
    uint32_t localTime = gTimezone.toLocal(mTimestamp);
    uint32_t nxtTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86400);  // next midnight local time
    onceAt(std::bind(&app::tickMidnight, this), nxtTrig, "mid2");

    Inverter<> *iv;
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter

        // reset alarms
        if(InverterStatus::OFF == iv->status)
            iv->resetAlarms();

        // clear max values
        if(mConfig->inst.rstMaxValsMidNight) {
            uint8_t pos;
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            for(uint8_t i = 0; i <= iv->channels; i++) {
                pos = iv->getPosByChFld(i, FLD_MP, rec);
                iv->setValue(pos, rec, 0.0f);
            }
        }
    }

    if (mConfig->inst.rstYieldMidNight) {
        zeroIvValues(!CHECK_AVAIL, !SKIP_YIELD_DAY);

        if (mMqttEnabled)
            mMqtt.tickerMidnight();
    }
}

//-----------------------------------------------------------------------------
void app::tickSend(void) {
    if(mConfig->nrf.enabled) {
        if(!mNrfRadio.isChipConnected()) {
            DPRINTLN(DBG_WARN, F("NRF24 not connected!"));
            return;
        }
    }
    if (mIVCommunicationOn) {
        if (!mNrfRadio.mBufCtrl.empty()) {
            if (mConfig->serial.debug) {
                DPRINT(DBG_DEBUG, F("recbuf not empty! #"));
                DBGPRINTLN(String(mNrfRadio.mBufCtrl.size()));
            }
        }
        #if defined(ESP32)
        if (!mCmtRadio.mBufCtrl.empty()) {
            if (mConfig->serial.debug) {
                DPRINT(DBG_INFO, F("recbuf not empty! #"));
                DBGPRINTLN(String(mCmtRadio.mBufCtrl.size()));
            }
        }
        #endif

        int8_t maxLoop = MAX_NUM_INVERTERS;
        Inverter<> *iv = mSys.getInverterByPos(mSendLastIvId);
        while(maxLoop > 0) {
            do {
                mSendLastIvId = ((MAX_NUM_INVERTERS - 1) == mSendLastIvId) ? 0 : mSendLastIvId + 1;
                iv = mSys.getInverterByPos(mSendLastIvId);
            } while ((NULL == iv) && ((maxLoop--) > 0));
            if(NULL != iv)
                if(iv->config->enabled)
                    break;
        }

        if (NULL != iv) {
            if (iv->config->enabled) {
                if(mConfig->nrf.enabled) {
                    if (iv->ivGen == IV_HM)
                        mPayload.ivSend(iv);
                    else if(iv->ivGen == IV_MI)
                        mMiPayload.ivSend(iv);
                }
                #if defined(ESP32)
                if(mConfig->cmt.enabled) {
                    if((iv->ivGen == IV_HMS) || (iv->ivGen == IV_HMT))
                        mHmsPayload.ivSend(iv);
                }
                #endif
            }
        }
    } else {
        if (mConfig->serial.debug)
            DPRINTLN(DBG_WARN, F("Time not set or it is night time, therefore no communication to the inverter!"));
    }
    yield();

    updateLed();

    #if defined(ESP32)
    zeroexport();
    #endif
}

//-----------------------------------------------------------------------------
void app:: zeroIvValues(bool checkAvail, bool skipYieldDay) {
    Inverter<> *iv;
    bool changed = false;
    // set values to zero, except yields
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter
        if (!iv->config->enabled)
            continue;  // skip to next inverter

        if (checkAvail) {
            if (!iv->isAvailable())
                continue;
        }

        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        for(uint8_t ch = 0; ch <= iv->channels; ch++) {
            uint8_t pos = 0;
            for(uint8_t fld = 0; fld < FLD_EVT; fld++) {
                switch(fld) {
                    case FLD_YD:
                        if(skipYieldDay)
                            continue;
                        else
                            break;
                    case FLD_YT:
                        continue;
                }
                pos = iv->getPosByChFld(ch, fld, rec);
                iv->setValue(pos, rec, 0.0f);
            }
            // zero max power
            if(!skipYieldDay) {
                pos = iv->getPosByChFld(ch, FLD_MP, rec);
                iv->setValue(pos, rec, 0.0f);
            }

            iv->doCalculations();
        }
        changed = true;
    }

    if(changed) {
        if(mMqttEnabled && !skipYieldDay)
            mMqtt.setZeroValuesEnable();
        payloadEventListener(RealTimeRunData_Debug, NULL);
    }
}

//-----------------------------------------------------------------------------
void app::resetSystem(void) {
    snprintf(mVersion, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

#ifdef AP_ONLY
    mTimestamp = 1;
#endif

    mSendFirst = true;

    mSunrise = 0;
    mSunset  = 0;

    mMqttEnabled = false;

    mSendLastIvId = 0;
    mShowRebootRequest = false;
    mIVCommunicationOn = true;
    mSavePending = false;
    mSaveReboot = false;

    mNetworkConnected = false;

    memset(&mNrfStat, 0, sizeof(statistics_t));
    memset(&mCmtStat, 0, sizeof(statistics_t));
}

//-----------------------------------------------------------------------------
void app::mqttSubRxCb(JsonObject obj) {
    mApi.ctrlRequest(obj);
}

//-----------------------------------------------------------------------------
void app::setupLed(void) {
    uint8_t led_off = (mConfig->led.led_high_active) ? LOW : HIGH;

    if (mConfig->led.led0 != DEF_PIN_OFF) {
        pinMode(mConfig->led.led0, OUTPUT);
        digitalWrite(mConfig->led.led0, led_off);
    }
    if (mConfig->led.led1 != DEF_PIN_OFF) {
        pinMode(mConfig->led.led1, OUTPUT);
        digitalWrite(mConfig->led.led1, led_off);
    }
}

//-----------------------------------------------------------------------------
void app::updateLed(void) {
    uint8_t led_off = (mConfig->led.led_high_active) ? LOW : HIGH;
    uint8_t led_on = (mConfig->led.led_high_active) ? HIGH : LOW;

    if (mConfig->led.led0 != DEF_PIN_OFF) {
        Inverter<> *iv;
        for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
            iv = mSys.getInverterByPos(id);
            if (NULL != iv) {
                if (iv->isProducing()) {
                    // turn on when at least one inverter is producing
                    digitalWrite(mConfig->led.led0, led_on);
                    break;
                }
                else if(iv->config->enabled)
                    digitalWrite(mConfig->led.led0, led_off);
            }
        }
    }

    if (mConfig->led.led1 != DEF_PIN_OFF) {
        if (getMqttIsConnected()) {
            digitalWrite(mConfig->led.led1, led_on);
        } else {
            digitalWrite(mConfig->led.led1, led_off);
        }
    }
}
//-----------------------------------------------------------------------------
#if defined(ESP32)
void app::zeroexport() {
    if (!mConfig->plugin.zexport.enabled) return;
    if (!mConfig->plugin.zexport.rdytoSend) return;

    DynamicJsonDocument doc(512);
    JsonObject object = doc.to<JsonObject>();


    object["path"] = "ctrl";
    object["id"] = mConfig->plugin.zexport.Iv;
    object["val"] = round(mzExport.getPowertoSetnewValue());
    object["cmd"] = "limit_nonpersistent_absolute";

    String data;
    serializeJsonPretty(object, data);
    DPRINTLN(DBG_INFO, data);

    mApi.ctrlRequest(object);
}
#endif