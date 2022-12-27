//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
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


    #if !defined(AP_ONLY)
    mMqtt.setup(&mConfig->mqtt, mConfig->sys.deviceName, mVersion, mSys, &mTimestamp);
    #endif

    mWifi.setup(mConfig, &mTimestamp);
    #if !defined(AP_ONLY)
    everySec(std::bind(&ahoywifi::tickWifiLoop, &mWifi));
    #endif

    mSendTickerId = every(std::bind(&app::tickSend, this), mConfig->nrf.sendInterval);
    #if !defined(AP_ONLY)
        once(std::bind(&app::tickNtpUpdate, this), 2);
    #endif

    mSys->addInverters(&mConfig->inst);
    mPayload.setup(mSys);
    mPayload.enableSerialDebug(mConfig->serial.debug);

    if(!mSys->Radio.isChipConnected())
        DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));

    // when WiFi is in client mode, then enable mqtt broker
    #if !defined(AP_ONLY)
    if (mConfig->mqtt.broker[0] > 0) {
        everySec(std::bind(&PubMqttType::tickerSecond, &mMqtt));
        everyMin(std::bind(&PubMqttType::tickerMinute, &mMqtt));
        mMqtt.setSubscriptionCb(std::bind(&app::mqttSubRxCb, this, std::placeholders::_1));
    }
    #endif
    setupLed();

    mWeb.setup(this, mSys, mConfig);
    mWeb.setProtection(strlen(mConfig->sys.adminPwd) != 0);
    everySec(std::bind(&WebType::tickSecond, &mWeb));

    mApi.setup(this, mSys, mWeb.getWebSrvPtr(), mConfig);

    // Plugins
    #if defined(ENA_NOKIA) || defined(ENA_SSD1306)
    mMonoDisplay.setup(mSys, &mTimestamp);
    everySec(std::bind(&MonoDisplayType::tickerSecond, &mMonoDisplay));
    #endif

    mPubSerial.setup(mConfig, mSys, &mTimestamp);
    every(std::bind(&PubSerialType::tick, &mPubSerial), mConfig->serial.interval);
    //everySec(std::bind(&app::tickSerial, this));
}

//-----------------------------------------------------------------------------
void app::loop(void) {
    DPRINTLN(DBG_VERBOSE, F("app::loop"));

    ah::Scheduler::loop();
    mSys->Radio.loop();

    yield();

    if (ah::checkTicker(&mRxTicker, 5)) {
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
            mPayload.process(true, mConfig->nrf.maxRetransPerPyld, &mStat);
    }

#if !defined(AP_ONLY)
    mMqtt.loop();
#endif
}

//-----------------------------------------------------------------------------
void app::tickNtpUpdate(void) {
    uint32_t nxtTrig = 5;  // default: check again in 5 sec
    if (mWifi.getNtpTime()) {
        nxtTrig = 43200;    // check again in 12 h
        if((mSunrise == 0) && (mConfig->sun.lat) && (mConfig->sun.lon)) {
            mCalculatedTimezoneOffset = (int8_t)((mConfig->sun.lon >= 0 ? mConfig->sun.lon + 7.5 : mConfig->sun.lon - 7.5) / 15) * 3600;
            tickCalcSunrise();
        }
    }
    once(std::bind(&app::tickNtpUpdate, this), nxtTrig);
}

//-----------------------------------------------------------------------------
void app::tickCalcSunrise(void) {
    ah::calculateSunriseSunset(mTimestamp, mCalculatedTimezoneOffset, mConfig->sun.lat, mConfig->sun.lon, &mSunrise, &mSunset);
    tickIVCommunication();

    uint32_t nxtTrig = mTimestamp - ((mTimestamp - 10) % 86400) + 86400; // next midnight, -10 for safety that it is certain next day
    onceAt(std::bind(&app::tickCalcSunrise, this), nxtTrig);
    if (mConfig->mqtt.broker[0] > 0) {
        mMqtt.tickerSun(mSunrise, mSunset, mConfig->sun.offsetSec, mConfig->sun.disNightCom);
    }
}

//-----------------------------------------------------------------------------
void app::tickIVCommunication(void) {
    mIVCommunicationOn = !mConfig->sun.disNightCom; // if sun.disNightCom is false, communication is always on
    if (!mIVCommunicationOn) {  // inverter communication only during the day
        uint32_t nxtTrig;
        if (mTimestamp < (mSunrise - mConfig->sun.offsetSec)) { // current time is before communication start, set next trigger to communication start
            nxtTrig = mSunrise - mConfig->sun.offsetSec;
        } else {
            if (mTimestamp > (mSunset + mConfig->sun.offsetSec)) { // current time is past communication stop, nothing to do. Next update will be done at midnight by tickCalcSunrise
                return;
            } else { // current time lies within communication start/stop time, set next trigger to communication stop
                mIVCommunicationOn = true;
                nxtTrig = mSunset + mConfig->sun.offsetSec;
            }
        }
        onceAt(std::bind(&app::tickIVCommunication, this), nxtTrig);
    }
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
            if(iv->config->enabled) {
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

                mPayload.reset(iv, mTimestamp);
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
                    iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
                } else {
                    uint8_t cmd = iv->getQueuedCmd();
                    DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") sendTimePacket"));
                    mSys->Radio.sendTimePacket(iv->radioId.u64, cmd, mPayload.getTs(iv), iv->alarmMesIndex);
                    mPayload.setTxCmd(iv, cmd);
                    mRxTicker = 0;
                }
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
void app::handleIntr(void) {
    DPRINTLN(DBG_VERBOSE, F("app::handleIntr"));
    mSys->Radio.handleIntr();
}

//-----------------------------------------------------------------------------
void app::resetSystem(void) {
    snprintf(mVersion, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

#ifdef AP_ONLY
    mTimestamp = 1;
#else
    mTimestamp = 0;
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
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            if(iv->isProducing(mTimestamp, rec))
                digitalWrite(mConfig->led.led0, LOW); // LED on
            else
                digitalWrite(mConfig->led.led0, HIGH); // LED off
        }
    }
}
