//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
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
    DPRINTLN(DBG_INFO, F("Settings valid: ") + String((mSettings.getValid()) ? F("true") : F("false")));

    mSys = new HmSystemType();
    mSys->enableDebug();
    mSys->setup(mConfig->nrf.amplifierPower, mConfig->nrf.pinIrq, mConfig->nrf.pinCe, mConfig->nrf.pinCs);
    mPayload.addListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1));

    #if defined(AP_ONLY)
    mInnerLoopCb = std::bind(&app::loopStandard, this);
    #else
    mInnerLoopCb = std::bind(&app::loopWifi, this);
    #endif

    mWifi.setup(mConfig, &mTimestamp, std::bind(&app::onWifi, this, std::placeholders::_1));
    #if !defined(AP_ONLY)
    everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi), "wifiL");
    #endif

    mSys->addInverters(&mConfig->inst);
    mPayload.setup(this, mSys, &mStat, mConfig->nrf.maxRetransPerPyld, &mTimestamp);
    mPayload.enableSerialDebug(mConfig->serial.debug);

    if(!mSys->Radio.isChipConnected())
        DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));

    // when WiFi is in client mode, then enable mqtt broker
    #if !defined(AP_ONLY)
    mMqttEnabled = (mConfig->mqtt.broker[0] > 0);
    if (mMqttEnabled) {
        mMqtt.setup(&mConfig->mqtt, mConfig->sys.deviceName, mVersion, mSys, &mTimestamp);
        mMqtt.setSubscriptionCb(std::bind(&app::mqttSubRxCb, this, std::placeholders::_1));
    }
    #endif
    setupLed();

    mWeb.setup(this, mSys, mConfig);
    mWeb.setProtection(strlen(mConfig->sys.adminPwd) != 0);

    mApi.setup(this, mSys, mWeb.getWebSrvPtr(), mConfig);

    // Plugins
    #if defined(ENA_NOKIA) || defined(ENA_SSD1306) || defined(ENA_SH1106)
    mMonoDisplay.setup(&mConfig->plugin.display, mSys, &mTimestamp);
    #endif

    mPubSerial.setup(mConfig, mSys, &mTimestamp);

    regularTickers();
}

//-----------------------------------------------------------------------------
void app::loop(void) {
    mInnerLoopCb();
}

//-----------------------------------------------------------------------------
void app::loopStandard(void) {
    ah::Scheduler::loop();

    mSys->Radio.loop();
    mPayload.loop();

    yield();

    if (ah::checkTicker(&mRxTicker, 4)) {
        bool rxRdy = mSys->Radio.switchRxCh();

        if (!mSys->BufCtrl.empty()) {
            uint8_t len;
            packet_t *p = mSys->BufCtrl.getBack();

            if (mSys->Radio.checkPaketCrc(p->packet, &len, p->rxCh)) {
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
            mPayload.process(true);
    }

#if !defined(AP_ONLY)
    if(mMqttEnabled)
        mMqtt.loop();
#endif
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
    regularTickers();   // reinstall regular tickers
    if (gotIp) {
        mInnerLoopCb = std::bind(&app::loopStandard, this);
        mSendTickerId = every(std::bind(&app::tickSend, this), mConfig->nrf.sendInterval, "tSend");
        mMqttReconnect = true;
        once(std::bind(&app::tickNtpUpdate, this), 2, "ntp2");
    }
    else {
        mInnerLoopCb = std::bind(&app::loopWifi, this);
        everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi), "wifiL");
    }
}

//-----------------------------------------------------------------------------
void app::regularTickers(void) {
    DPRINTLN(DBG_DEBUG, F("regularTickers"));
    everySec(std::bind(&WebType::tickSecond, &mWeb), "webSc");
    // Plugins
    #if defined(ENA_NOKIA) || defined(ENA_SSD1306) || defined(ENA_SH1106)
    everySec(std::bind(&MonoDisplayType::tickerSecond, &mMonoDisplay), "disp");
    #endif
    every(std::bind(&PubSerialType::tick, &mPubSerial), mConfig->serial.interval, "uart");
}

//-----------------------------------------------------------------------------
void app::tickNtpUpdate(void) {
    uint32_t nxtTrig = 5;  // default: check again in 5 sec
    if (mWifi.getNtpTime(&nxtTrig)) {
        if (mMqttReconnect && mMqttEnabled) {
            mMqtt.connect();
            everySec(std::bind(&PubMqttType::tickerSecond, &mMqtt), "mqttS");
            everyMin(std::bind(&PubMqttType::tickerMinute, &mMqtt), "mqttM");
            uint32_t nxtTrig = mTimestamp - ((mTimestamp - 1) % 86400) + 86400; // next midnight
            if(mConfig->mqtt.rstYieldMidNight)
                onceAt(std::bind(&app::tickMidnight, this), nxtTrig, "midNi");
            mMqttReconnect = false;
        }

        nxtTrig = 43200;

        if((mSunrise == 0) && (mConfig->sun.lat) && (mConfig->sun.lon)) {
            mCalculatedTimezoneOffset = (int8_t)((mConfig->sun.lon >= 0 ? mConfig->sun.lon + 7.5 : mConfig->sun.lon - 7.5) / 15) * 3600;
            tickCalcSunrise();
        }
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
    if (mMqttEnabled)
        tickComm();
}

//-----------------------------------------------------------------------------
void app::tickSun(void) {
    // only used and enabled by MQTT (see setup())
    if (!mMqtt.tickerSun(mSunrise, mSunset, mConfig->sun.offsetSec, mConfig->sun.disNightCom))
        once(std::bind(&app::tickSun, this), 1, "mqSun");    // MQTT not connected, retry
}

//-----------------------------------------------------------------------------
void app::tickComm(void) {
    // only used and enabled by MQTT (see setup())
    if (!mMqtt.tickerComm(!mIVCommunicationOn))
        once(std::bind(&app::tickComm, this), 1, "mqCom");    // MQTT not connected, retry
}

//-----------------------------------------------------------------------------
void app::tickSend(void) {
    if(!mSys->Radio.isChipConnected()) {
        DPRINTLN(DBG_WARN, "NRF24 not connected!");
        return;
    }
    if (mIVCommunicationOn) {
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
            if(iv->config->enabled)
                mPayload.ivSend(iv);
        }
    } else {
        if (mConfig->serial.debug)
            DPRINTLN(DBG_WARN, F("Time not set or it is night time, therefore no communication to the inverter!"));
    }
    yield();

    updateLed();
}

//-----------------------------------------------------------------------------
void app::tickMidnight(void) {
    // only used and enabled by MQTT (see setup())
    uint32_t nxtTrig = mTimestamp - ((mTimestamp - 1) % 86400) + 86400; // next midnight
    onceAt(std::bind(&app::tickMidnight, this), nxtTrig, "mid2");

    mMqtt.tickerMidnight();
}

//-----------------------------------------------------------------------------
void app::handleIntr(void) {
    DPRINTLN(DBG_VERBOSE, F("app::handleIntr"));
    mSys->Radio.handleIntr();
}

//-----------------------------------------------------------------------------
void app::resetSystem(void) {
    snprintf(mVersion, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

#ifdef AP_ONLY
    mTimestamp = 1;
#endif

    mSunrise = 0;
    mSunset  = 0;

    mRxTicker = 0;
    mSendTickerId = 0xff; // invalid id

    mSendLastIvId = 0;
    mShowRebootRequest = false;
    mIVCommunicationOn = true;

    memset(&mStat, 0, sizeof(statistics_t));
}

//-----------------------------------------------------------------------------
void app::mqttSubRxCb(JsonObject obj) {
    mApi.ctrlRequest(obj);
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
            if(iv->isProducing(mTimestamp))
                digitalWrite(mConfig->led.led0, LOW); // LED on
            else
                digitalWrite(mConfig->led.led0, HIGH); // LED off
        }
    }
}
