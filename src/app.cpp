//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#include "app.h"
#include <ArduinoJson.h>
#include "utils/sun.h"

//-----------------------------------------------------------------------------
app::app() : ah::Scheduler() {}


//-----------------------------------------------------------------------------
void app::setup() {
    Serial.begin(115200);
    while (!Serial)
        yield();

    ah::Scheduler::setup();

    resetSystem();

    mSettings.setup();
    mSettings.getPtr(mConfig);
    DPRINT(DBG_INFO, F("Settings valid: "));
    if (mSettings.getValid())
        DBGPRINTLN(F("true"));
    else
        DBGPRINTLN(F("false"));

    if(mConfig->nrf.enabled) {
        mNrfRadio.setup(mConfig->nrf.amplifierPower, mConfig->nrf.pinIrq, mConfig->nrf.pinCe, mConfig->nrf.pinCs, mConfig->nrf.pinSclk, mConfig->nrf.pinMosi, mConfig->nrf.pinMiso);
        mNrfRadio.enableDebug();
    }
    #if defined(ESP32)
    if(mConfig->cmt.enabled) {
        mCmtRadio.setup(mConfig->cmt.pinCsb, mConfig->cmt.pinFcsb, false);
        mCmtRadio.enableDebug();
    }
    #endif

    #if defined(AP_ONLY)
    mInnerLoopCb = std::bind(&app::loopStandard, this);
    #else
    mInnerLoopCb = std::bind(&app::loopWifi, this);
    #endif

    mWifi.setup(mConfig, &mTimestamp, std::bind(&app::onWifi, this, std::placeholders::_1));
    #if !defined(AP_ONLY)
    everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi), "wifiL");
    #endif

    mSys.addInverters(&mConfig->inst);
    if(mConfig->nrf.enabled) {
        mPayload.setup(this, &mSys, &mNrfRadio, &mStat, mConfig->nrf.maxRetransPerPyld, &mTimestamp);
        mPayload.enableSerialDebug(mConfig->serial.debug);
        mPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1));

        mMiPayload.setup(this, &mSys, &mNrfRadio, &mStat, mConfig->nrf.maxRetransPerPyld, &mTimestamp);
        mMiPayload.enableSerialDebug(mConfig->serial.debug);
        mMiPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1));
    }

    #if defined(ESP32)
        mHmsPayload.setup(this, &mSys, &mCmtRadio, &mStat, 5, &mTimestamp);
        mHmsPayload.enableSerialDebug(mConfig->serial.debug);
        mHmsPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1));
    #endif
    /*DBGPRINTLN("--- after payload");
    DBGPRINTLN(String(ESP.getFreeHeap()));
    DBGPRINTLN(String(ESP.getHeapFragmentation()));
    DBGPRINTLN(String(ESP.getMaxFreeBlockSize()));*/

    if(mConfig->nrf.enabled) {
        if (!mNrfRadio.isChipConnected())
            DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));
    }

    // when WiFi is in client mode, then enable mqtt broker
    #if !defined(AP_ONLY)
    mMqttEnabled = (mConfig->mqtt.broker[0] > 0);
    if (mMqttEnabled) {
        mMqtt.setup(&mConfig->mqtt, mConfig->sys.deviceName, mVersion, &mSys, &mTimestamp);
        mMqtt.setSubscriptionCb(std::bind(&app::mqttSubRxCb, this, std::placeholders::_1));
        mPayload.addAlarmListener(std::bind(&PubMqttType::alarmEventListener, &mMqtt, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        mMiPayload.addAlarmListener(std::bind(&PubMqttType::alarmEventListener, &mMqtt, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    #endif
    setupLed();

    mWeb.setup(this, &mSys, mConfig);
    mWeb.setProtection(strlen(mConfig->sys.adminPwd) != 0);

    mApi.setup(this, &mSys, &mNrfRadio, mWeb.getWebSrvPtr(), mConfig);

    // Plugins
    if (mConfig->plugin.display.type != 0)
        mDisplay.setup(&mConfig->plugin.display, &mSys, &mTimestamp, mVersion);

    mPubSerial.setup(mConfig, &mSys, &mTimestamp);

    regularTickers();


    // DBGPRINTLN("--- end setup");
    // DBGPRINTLN(String(ESP.getFreeHeap()));
    // DBGPRINTLN(String(ESP.getHeapFragmentation()));
    // DBGPRINTLN(String(ESP.getMaxFreeBlockSize()));
}

//-----------------------------------------------------------------------------
void app::loop(void) {
    mInnerLoopCb();
}

//-----------------------------------------------------------------------------
void app::loopStandard(void) {
    ah::Scheduler::loop();

    if (mNrfRadio.loop() && mConfig->nrf.enabled) {
        while (!mNrfRadio.mBufCtrl.empty()) {
            packet_t *p = &mNrfRadio.mBufCtrl.front();

            if (mConfig->serial.debug) {
                DPRINT(DBG_INFO, F("RX "));
                DBGPRINT(String(p->len));
                DBGPRINT(F("B Ch"));
                DBGPRINT(String(p->ch));
                DBGPRINT(F(" | "));
                ah::dumpBuf(p->packet, p->len);
            }
            mStat.frmCnt++;

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
                DBGPRINT(F(" RSSI "));
                DBGPRINT(String(p->rssi));
                DBGPRINT(F("dBm | "));
                ah::dumpBuf(&p->data[1], p->data[0]);
            }
            mStat.frmCnt++;

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

    if (mMqttEnabled)
        mMqtt.loop();
}

//-----------------------------------------------------------------------------
void app::loopWifi(void) {
    ah::Scheduler::loop();
    yield();
}

//-----------------------------------------------------------------------------
void app::onWifi(bool gotIp) {
    DPRINTLN(DBG_DEBUG, F("onWifi"));
    ah::Scheduler::resetTicker();
    regularTickers();  // reinstall regular tickers
    if (gotIp) {
        mInnerLoopCb = std::bind(&app::loopStandard, this);
        every(std::bind(&app::tickSend, this), mConfig->nrf.sendInterval, "tSend");
        #if defined(ESP32)
        if(mConfig->cmt.enabled)
            everySec(std::bind(&CmtRadioType::tickSecond, &mCmtRadio), "tsCmt");
        #endif
        mMqttReconnect = true;
        mSunrise = 0;  // needs to be set to 0, to reinstall sunrise and ivComm tickers!
        once(std::bind(&app::tickNtpUpdate, this), 2, "ntp2");
        if (WIFI_AP == WiFi.getMode()) {
            mMqttEnabled = false;
            everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi), "wifiL");
        }
    } else {
        mInnerLoopCb = std::bind(&app::loopWifi, this);
        everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi), "wifiL");
    }
}

//-----------------------------------------------------------------------------
void app::regularTickers(void) {
    DPRINTLN(DBG_DEBUG, F("regularTickers"));
    everySec(std::bind(&WebType::tickSecond, &mWeb), "webSc");
    // Plugins
    if (mConfig->plugin.display.type != 0)
        everySec(std::bind(&DisplayType::tickerSecond, &mDisplay), "disp");
    every(std::bind(&PubSerialType::tick, &mPubSerial), mConfig->serial.interval, "uart");

    // every([this]() {mPayload.simulation();}, 15, "simul");
}

//-----------------------------------------------------------------------------
void app::tickNtpUpdate(void) {
    uint32_t nxtTrig = 5;  // default: check again in 5 sec
    bool isOK = mWifi.getNtpTime();
    if (isOK || mTimestamp != 0) {
        if (mMqttReconnect && mMqttEnabled) {
            mMqtt.tickerSecond();
            everySec(std::bind(&PubMqttType::tickerSecond, &mMqtt), "mqttS");
            everyMin(std::bind(&PubMqttType::tickerMinute, &mMqtt), "mqttM");
        }

        // only install schedulers once even if NTP wasn't successful in first loop
        if (mMqttReconnect) {  // @TODO: mMqttReconnect is variable which scope has changed
            if (mConfig->inst.rstValsNotAvail)
                everyMin(std::bind(&app::tickMinute, this), "tMin");
            if (mConfig->inst.rstYieldMidNight) {
                uint32_t localTime = gTimezone.toLocal(mTimestamp);
                uint32_t midTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86400);  // next midnight local time
                onceAt(std::bind(&app::tickMidnight, this), midTrig, "midNi");
            }
        }

        nxtTrig = isOK ? 43200 : 60;  // depending on NTP update success check again in 12 h or in 1 min

        if ((mSunrise == 0) && (mConfig->sun.lat) && (mConfig->sun.lon)) {
            mCalculatedTimezoneOffset = (int8_t)((mConfig->sun.lon >= 0 ? mConfig->sun.lon + 7.5 : mConfig->sun.lon - 7.5) / 15) * 3600;
            tickCalcSunrise();
        }

        // immediately start communicating
        // @TODO: leads to reboot loops? not sure #674
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
    Inverter<> *iv;
    bool changed = false;
    // set values to zero, except yields
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter

        mPayload.zeroInverterValues(iv);
        changed = true;
    }

    if(changed)
        payloadEventListener(RealTimeRunData_Debug);
}

//-----------------------------------------------------------------------------
void app::tickMinute(void) {
    // only triggered if 'reset values on no avail is enabled'

    Inverter<> *iv;
    bool changed = false;
    // set values to zero, except yields
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter

        if (!iv->isAvailable(mTimestamp) && !iv->isProducing(mTimestamp) && iv->config->enabled) {
            mPayload.zeroInverterValues(iv);
            changed = true;
        }
    }

    if(changed)
        payloadEventListener(RealTimeRunData_Debug);
}

//-----------------------------------------------------------------------------
void app::tickMidnight(void) {
    // only triggered if 'reset values at midnight is enabled'
    uint32_t localTime = gTimezone.toLocal(mTimestamp);
    uint32_t nxtTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86400);  // next midnight local time
    onceAt(std::bind(&app::tickMidnight, this), nxtTrig, "mid2");

    Inverter<> *iv;
    bool changed = false;
    // set values to zero, except yield total
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter

        mPayload.zeroInverterValues(iv, false);
        changed = true;
    }

    if(changed)
        payloadEventListener(RealTimeRunData_Debug);

    if (mMqttEnabled)
        mMqtt.tickerMidnight();
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
        do {
            mSendLastIvId = ((MAX_NUM_INVERTERS - 1) == mSendLastIvId) ? 0 : mSendLastIvId + 1;
            iv = mSys.getInverterByPos(mSendLastIvId);
        } while ((NULL == iv) && ((maxLoop--) > 0));

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

    memset(&mStat, 0, sizeof(statistics_t));
}

//-----------------------------------------------------------------------------
void app::mqttSubRxCb(JsonObject obj) {
    mApi.ctrlRequest(obj);
}

//-----------------------------------------------------------------------------
void app::setupLed(void) {
    uint8_t led_off = (mConfig->led.led_high_active) ? LOW : HIGH;

    if (mConfig->led.led0 != 0xff) {
        pinMode(mConfig->led.led0, OUTPUT);
        digitalWrite(mConfig->led.led0, led_off);
    }
    if (mConfig->led.led1 != 0xff) {
        pinMode(mConfig->led.led1, OUTPUT);
        digitalWrite(mConfig->led.led1, led_off);
    }
}

//-----------------------------------------------------------------------------
void app::updateLed(void) {
    uint8_t led_off = (mConfig->led.led_high_active) ? LOW : HIGH;
    uint8_t led_on = (mConfig->led.led_high_active) ? HIGH : LOW;

    if (mConfig->led.led0 != 0xff) {
        Inverter<> *iv = mSys.getInverterByPos(0);
        if (NULL != iv) {
            if (iv->isProducing(mTimestamp))
                digitalWrite(mConfig->led.led0, led_on);
            else
                digitalWrite(mConfig->led.led0, led_off);
        }
    }

    if (mConfig->led.led1 != 0xff) {
        if (getMqttIsConnected()) {
            digitalWrite(mConfig->led.led1, led_on);
        } else {
            digitalWrite(mConfig->led.led1, led_off);
        }
    }
}
