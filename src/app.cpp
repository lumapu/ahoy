//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#include <ArduinoJson.h>
#include "app.h"
#include "utils/sun.h"

#if !defined(ESP32)
    void esp_task_wdt_reset() {}
#endif


//-----------------------------------------------------------------------------
app::app()
    : ah::Scheduler {}
    , mSunrise {0}
    , mSunset  {0}
{
    memset(mVersion, 0, sizeof(char) * 12);
    memset(mVersionModules, 0, sizeof(char) * 12);
}


//-----------------------------------------------------------------------------
void app::setup() {
    Serial.begin(115200);
    while (!Serial)
        yield();

    #if defined(ESP32)
        esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
        esp_task_wdt_add(NULL);
    #endif

    resetSystem();
    esp_task_wdt_reset();

    mSettings.setup(mConfig);
    ah::Scheduler::setup(mConfig->inst.startWithoutTime);
    DPRINT(DBG_INFO, F("Settings valid: "));
    DBGPRINTLN(mConfig->valid ? F("true") : F("false"));

    esp_task_wdt_reset();

    mNrfRadio.setup(&mConfig->serial.debug, &mConfig->serial.privacyLog, &mConfig->serial.printWholeTrace, &mConfig->nrf);
    #if defined(ESP32)
    mCmtRadio.setup(&mConfig->serial.debug, &mConfig->serial.privacyLog, &mConfig->serial.printWholeTrace, &mConfig->cmt, mConfig->sys.region);
    #endif

    #ifdef ETHERNET
        delay(1000);
        mNetwork = static_cast<AhoyNetwork*>(new AhoyEthernet());
    #else
        mNetwork = static_cast<AhoyNetwork*>(new AhoyWifi());
    #endif
    mNetwork->setup(mConfig, [this](bool gotIp) { this->onNetwork(gotIp); }, [this](uint32_t gotTime) { this->onNtpUpdate(gotTime); });
    mNetwork->begin();

    esp_task_wdt_reset();

    mCommunication.setup(&mTimestamp, &mConfig->serial.debug, &mConfig->serial.privacyLog, &mConfig->serial.printWholeTrace);
    mCommunication.addPayloadListener([this] (uint8_t cmd, Inverter<> *iv) { payloadEventListener(cmd, iv); });
    #if defined(ENABLE_MQTT)
        mCommunication.addPowerLimitAckListener([this] (Inverter<> *iv) { mMqtt.setPowerLimitAck(iv); });
    #endif
    mSys.setup(&mTimestamp, &mConfig->inst, this);
    for (uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
        initInverter(i);
    }

    if(mConfig->nrf.enabled) {
        if (!mNrfRadio.isChipConnected())
            DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));
    }

    esp_task_wdt_reset();

    // when WiFi is in client mode, then enable mqtt broker
    #if defined(ENABLE_MQTT)
    mMqttEnabled = (mConfig->mqtt.broker[0] > 0);
    if (mMqttEnabled) {
        mMqtt.setup(this, &mConfig->mqtt, mConfig->sys.deviceName, mVersion, &mSys, &mTimestamp, &mUptime);
        mMqtt.setSubscriptionCb([this](JsonObject obj) { mqttSubRxCb(obj); });
        mCommunication.addAlarmListener([this](Inverter<> *iv) { mMqtt.alarmEvent(iv); });
    }
    #endif
    setupLed();

    esp_task_wdt_reset();

    mWeb.setup(this, &mSys, mConfig);
    mApi.setup(this, &mSys, mWeb.getWebSrvPtr(), mConfig);
    mProtection = Protection::getInstance(mConfig->sys.adminPwd);

    #ifdef ENABLE_SYSLOG
    mDbgSyslog.setup(mConfig); // be sure to init after mWeb.setup (webSerial uses also debug callback)
    #endif
    // Plugins
    mMaxPower.setup(&mTimestamp, mConfig->inst.sendInterval);
    #if defined(PLUGIN_DISPLAY)
    if (DISP_TYPE_T0_NONE != mConfig->plugin.display.type)
        #if defined(ESP32)
        mDisplay.setup(this, &mConfig->plugin.display, &mSys, &mNrfRadio, &mCmtRadio, &mTimestamp);
        #else
        mDisplay.setup(this, &mConfig->plugin.display, &mSys, &mNrfRadio, NULL, &mTimestamp);
        #endif
    #endif

    esp_task_wdt_reset();

    #if defined(ENABLE_HISTORY)
    mHistory.setup(this, &mSys, mConfig, &mTimestamp);
    #endif /*ENABLE_HISTORY*/

    mPubSerial.setup(mConfig, &mSys, &mTimestamp);

    //mImprov.setup(this, mConfig->sys.deviceName, mVersion);

    #if defined(ENABLE_SIMULATOR)
    mSimulator.setup(&mSys, &mTimestamp, 0);
    mSimulator.addPayloadListener([this](uint8_t cmd, Inverter<> *iv) { payloadEventListener(cmd, iv); });
    #endif /*ENABLE_SIMULATOR*/

    esp_task_wdt_reset();
    regularTickers();
}

//-----------------------------------------------------------------------------
void app::loop(void) {
    esp_task_wdt_reset();

    mNrfRadio.loop();

    #if defined(ESP32)
    mCmtRadio.loop();
    #endif

    ah::Scheduler::loop();
    mCommunication.loop();

    #if defined(ENABLE_MQTT)
    if (mMqttEnabled && mNetworkConnected)
        mMqtt.loop();
    #endif

    #if defined(PLUGIN_DISPLAY)
    mDisplay.loop();
    #endif
    yield();
}

//-----------------------------------------------------------------------------
void app::onNetwork(bool connected) {
    mNetworkConnected = connected;
    #if defined(ENABLE_MQTT)
    if (mMqttEnabled) {
        resetTickerByName("mqttS");
        resetTickerByName("mqttM");
    }
    #endif

    if(connected) {
        mNetwork->updateNtpTime();

        resetTickerByName("tSend");
        every([this]() { tickSend(); }, mConfig->inst.sendInterval, "tSend");

        #if defined(ENABLE_MQTT)
        if (mMqttEnabled) {
            everySec([this]() { mMqtt.tickerSecond(); }, "mqttS");
            everyMin([this]() { mMqtt.tickerMinute(); }, "mqttM");
        }
        #endif /*ENABLE_MQTT*/
    }
}

//-----------------------------------------------------------------------------
void app::regularTickers(void) {
    DPRINTLN(DBG_DEBUG, F("regularTickers"));
    everySec([this]() { mWeb.tickSecond(); }, "webSc");
    everySec([this]() { mProtection->tickSecond(); }, "prot");
    everySec([this]() { mNetwork->tickNetworkLoop(); }, "net");

    if(mConfig->inst.startWithoutTime)
        every([this]() { tickSend(); }, mConfig->inst.sendInterval, "tSend");


    every([this]() { mNetwork->updateNtpTime(); }, mConfig->ntp.interval * 60, "ntp");

    if (mConfig->inst.rstValsNotAvail)
        everyMin([this]() { tickMinute(); }, "tMin");

    // Plugins
    #if defined(PLUGIN_DISPLAY)
    if (DISP_TYPE_T0_NONE != mConfig->plugin.display.type)
        everySec([this]() { mDisplay.tickerSecond(); }, "disp");
    #endif
    every([this]() { mPubSerial.tick(); }, 5, "uart");
    //everySec([this]() { mImprov.tickSerial(); }, "impro");

    #if defined(ENABLE_HISTORY)
    everySec([this]() { mHistory.tickerSecond(); }, "hist");
    #endif /*ENABLE_HISTORY*/

    #if defined(ENABLE_SIMULATOR)
    every([this]() {mSimulator->tick(); }, 5, "sim");
    #endif /*ENABLE_SIMULATOR*/
}

//-----------------------------------------------------------------------------
void app::onNtpUpdate(uint32_t utcTimestamp) {
    if(0 == utcTimestamp) {
        // try again in 5s
        once([this]() { mNetwork->updateNtpTime(); }, 5, "ntp");
    } else {
        mTimestamp = utcTimestamp;
        DPRINTLN(DBG_INFO, "[NTP]: " + ah::getDateTimeStr(mTimestamp) + " UTC");

        uint32_t localTime = gTimezone.toLocal(mTimestamp);
        uint32_t midTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86400);  // next midnight local time
        resetTickerByName("midNi");
        onceAt([this]() { tickMidnight(); }, midTrig, "midNi");

        if (mConfig->sys.schedReboot) {
            uint32_t rebootTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86410);  // reboot 10 secs after midnght
            resetTickerByName("midRe");
            onceAt([this]() { tickReboot(); }, rebootTrig, "midRe");
        }

        if ((0 == mSunrise) && (0.0 != mConfig->sun.lat) && (0.0 != mConfig->sun.lon)) {
            mCalculatedTimezoneOffset = (int8_t)((mConfig->sun.lon >= 0 ? mConfig->sun.lon + 7.5 : mConfig->sun.lon - 7.5) / 15) * 3600;
            tickCalcSunrise();
        }
    }
}

//-----------------------------------------------------------------------------
void app::tickCalcSunrise(void) {
    if (mSunrise == 0)                                          // on boot/reboot calc sun values for current time
        ah::calculateSunriseSunset(mTimestamp, mCalculatedTimezoneOffset, mConfig->sun.lat, mConfig->sun.lon, &mSunrise, &mSunset);

    if (mTimestamp > (mSunset + mConfig->sun.offsetSecEvening))        // current time is past communication stop, calc sun values for next day
        ah::calculateSunriseSunset(mTimestamp + 86400, mCalculatedTimezoneOffset, mConfig->sun.lat, mConfig->sun.lon, &mSunrise, &mSunset);

    tickIVCommunication();

    uint32_t nxtTrig = mSunset + mConfig->sun.offsetSecEvening + 60;    // set next trigger to communication stop, +60 for safety that it is certain past communication stop
    onceAt([this]() { tickCalcSunrise(); }, nxtTrig, "Sunri");
    if (mMqttEnabled) {
        tickSun();
        nxtTrig = mSunrise + mConfig->sun.offsetSecMorning + 1;         // one second safety to trigger correctly
        onceAt([this]() { tickSunrise(); }, nxtTrig, "mqSr");    // trigger on sunrise to update 'dis_night_comm'
    }
}

//-----------------------------------------------------------------------------
void app::tickIVCommunication(void) {
    bool restartTick = false;
    bool zeroValues = false;
    uint32_t nxtTrig = 0;

    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        Inverter<> *iv = mSys.getInverterByPos(i);
        if(NULL == iv)
            continue;

        iv->commEnabled = !iv->config->disNightCom; // if sun.disNightCom is false, communication is always on
        if (!iv->commEnabled) {  // inverter communication only during the day
            if (mTimestamp < (mSunrise + mConfig->sun.offsetSecMorning)) { // current time is before communication start, set next trigger to communication start
                nxtTrig = mSunrise + mConfig->sun.offsetSecMorning;
            } else {
                if (mTimestamp >= (mSunset + mConfig->sun.offsetSecEvening)) { // current time is past communication stop, nothing to do. Next update will be done at midnight by tickCalcSunrise
                    nxtTrig = 0;
                } else { // current time lies within communication start/stop time, set next trigger to communication stop
                    if((!iv->commEnabled) && mConfig->inst.rstValsCommStart)
                        zeroValues = true;
                    iv->commEnabled = true;
                    nxtTrig = mSunset + mConfig->sun.offsetSecEvening;
                }
            }
            if (nxtTrig != 0)
                restartTick = true;
        }

        if ((!iv->commEnabled) && (mConfig->inst.rstValsCommStop))
            zeroValues = true;
    }

    if(restartTick) // at least one inverter
        onceAt([this]() { tickIVCommunication(); }, nxtTrig, "ivCom");

    if (zeroValues) // at least one inverter
        once([this]() { tickZeroValues(); }, mConfig->inst.sendInterval, "tZero");
}

//-----------------------------------------------------------------------------
void app::tickSun(void) {
    // only used and enabled by MQTT (see setup())
    #if defined(ENABLE_MQTT)
    if (!mMqtt.tickerSun(mSunrise, mSunset, mConfig->sun.offsetSecMorning, mConfig->sun.offsetSecEvening))
        once([this]() { tickSun(); }, 1, "mqSun");  // MQTT not connected, retry
    #endif
}

//-----------------------------------------------------------------------------
void app::tickSunrise(void) {
    // only used and enabled by MQTT (see setup())
    #if defined(ENABLE_MQTT)
    if (!mMqtt.tickerSun(mSunrise, mSunset, mConfig->sun.offsetSecMorning, mConfig->sun.offsetSecEvening, true))
        once([this]() { tickSun(); }, 1, "mqSun");  // MQTT not connected, retry
    #endif
}

//-----------------------------------------------------------------------------
void app::notAvailChanged(void) {
    #if defined(ENABLE_MQTT)
    if (mMqttEnabled)
        mMqtt.notAvailChanged(mAllIvNotAvail);
    #endif
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
    resetTickerByName("midNi");
    onceAt([this]() { tickMidnight(); }, nxtTrig, "midNi");

    Inverter<> *iv;
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter

        // reset alarms
        if(InverterStatus::OFF == iv->getStatus())
            iv->resetAlarms();
    }

    if (mConfig->inst.rstValsAtMidNight) {
        zeroIvValues(!CHECK_AVAIL, !SKIP_YIELD_DAY);

        #if defined(ENABLE_MQTT)
        if (mMqttEnabled)
            mMqtt.tickerMidnight();
        #endif
    }
}

//-----------------------------------------------------------------------------
void app::tickSend(void) {
    bool notAvail = true;
    uint8_t fill = mCommunication.getFillState();
    uint8_t max = mCommunication.getMaxFill();
    if((max-MAX_NUM_INVERTERS) <= fill) {
        DPRINT(DBG_WARN, F("send queue almost full, consider to increase interval, "));
        DBGPRINT(String(fill));
        DBGPRINT(F(" of "));
        DBGPRINT(String(max));
        DBGPRINTLN(F(" entries used"));
    }

    for (uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
        Inverter<> *iv = mSys.getInverterByPos(i);
        if(!sendIv(iv))
            notAvail = false;
    }

    if(mAllIvNotAvail != notAvail)
        once([this]() { notAvailChanged(); }, 1, "avail");
    mAllIvNotAvail = notAvail;

    updateLed();
}

//-----------------------------------------------------------------------------
bool app::sendIv(Inverter<> *iv) {
    if(NULL == iv)
        return true;

    if(!iv->config->enabled)
        return true;

    if(!iv->commEnabled) {
        DPRINT_IVID(DBG_INFO, iv->id);
        DBGPRINTLN(F("no communication to the inverter (night time)"));
        return true;
    }

    if(!iv->radio->isChipConnected())
        return true;

    bool notAvail = true;
    if(InverterStatus::OFF != iv->status)
        notAvail = false;

    iv->tickSend([this, iv](uint8_t cmd, bool isDevControl) {
        if(isDevControl)
            mCommunication.addImportant(iv, cmd);
        else
            mCommunication.add(iv, cmd);
    });

    return notAvail;
}

//-----------------------------------------------------------------------------
void app:: zeroIvValues(bool checkAvail, bool skipYieldDay) {
    Inverter<> *iv;
    bool changed = false;

    mMaxPower.reset();

    // set values to zero, except yields
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter
        if (!iv->config->enabled)
            continue;  // skip to next inverter

        if (checkAvail) {
            if (iv->isAvailable())
                continue;
        }

        changed = true;
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
            // zero max power and max temperature
            if(mConfig->inst.rstIncludeMaxVals) {
                pos = iv->getPosByChFld(ch, FLD_MP, rec);
                iv->setValue(pos, rec, 0.0f);
                pos = iv->getPosByChFld(ch, FLD_MT, rec);
                iv->setValue(pos, rec, 0.0f);
                iv->resetAlarms(true);
            } else
                iv->resetAlarms();
            iv->doCalculations();
        }
    }

    if(changed)
        payloadEventListener(RealTimeRunData_Debug, nullptr);
}

//-----------------------------------------------------------------------------
void app::resetSystem(void) {
    snprintf(mVersion, sizeof(mVersion), "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    snprintf(mVersionModules, sizeof(mVersionModules), "%s",
    #ifdef ENABLE_PROMETHEUS_EP
        "P"
    #endif

    #ifdef ENABLE_MQTT
        "M"
    #endif

    #ifdef PLUGIN_DISPLAY
        "D"
    #endif

    #ifdef ENABLE_HISTORY
        "H"
    #endif

    #ifdef AP_ONLY
        "A"
    #endif

    #ifdef ENABLE_SYSLOG
        "Y"
    #endif

    #ifdef ENABLE_SIMULATOR
        "S"
    #endif

        "-"
    #ifdef LANG_DE
        "de"
    #else
        "en"
    #endif
    );

#ifdef AP_ONLY
    mTimestamp = 1;
#endif

    mAllIvNotAvail = true;

    mMqttEnabled = false;

    mSendLastIvId = 0;
    mShowRebootRequest = false;
    mSavePending = false;
    mSaveReboot = false;

    mNetworkConnected = false;
}

//-----------------------------------------------------------------------------
void app::mqttSubRxCb(JsonObject obj) {
    mApi.ctrlRequest(obj);
}

//-----------------------------------------------------------------------------
void app::setupLed(void) {
    uint8_t led_off = (mConfig->led.high_active) ? 0 : 255;
    for(uint8_t i = 0; i < 3; i ++) {
        if (mConfig->led.led[i] != DEF_PIN_OFF) {
            pinMode(mConfig->led.led[i], OUTPUT);
            analogWrite(mConfig->led.led[i], led_off);
        }
    }
}

//-----------------------------------------------------------------------------
void app::updateLed(void) {
    uint8_t led_off = (mConfig->led.high_active) ? 0 : 255;
    uint8_t led_on = (mConfig->led.high_active) ? (mConfig->led.luminance) : (255-mConfig->led.luminance);

    if (mConfig->led.led[0] != DEF_PIN_OFF) {
        for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
            Inverter<> *iv = mSys.getInverterByPos(id);
            if (NULL != iv) {
                if (iv->isProducing()) {
                    // turn on when at least one inverter is producing
                    analogWrite(mConfig->led.led[0], led_on);
                    break;
                }
                else if(iv->config->enabled)
                    analogWrite(mConfig->led.led[0], led_off);
            }
        }
    }

    if (mConfig->led.led[1] != DEF_PIN_OFF) {
        if (getMqttIsConnected()) {
            analogWrite(mConfig->led.led[1], led_on);
        } else {
            analogWrite(mConfig->led.led[1], led_off);
        }
    }

    if (mConfig->led.led[2] != DEF_PIN_OFF) {
        if((mTimestamp > (mSunset + mConfig->sun.offsetSecEvening)) || (mTimestamp < (mSunrise + mConfig->sun.offsetSecMorning)))
            analogWrite(mConfig->led.led[2], led_on);
        else
            analogWrite(mConfig->led.led[2], led_off);
    }
}
