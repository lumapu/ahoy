//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#include "app.h"
#include <ArduinoJson.h>
#include "utils/sun.h"
#include "plugins/SML_OBIS_Parser.h"

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

//-----------------------------------------------------------------------------
app::app() : ah::Scheduler() {}

//-----------------------------------------------------------------------------
void app::setup() {
#ifdef AHOY_SML_OBIS_SUPPORT
    /* Assumptions made:
       Electricity meter sends SML telegrams via IR interface (9600,8,n,1) without being asked (typical behaviour).
       An IR sensor is connected to the UART0 of AHOY DTU. Connected pins: GND-GND, 3V3-VCC, RX-RX, TX-TX.
    */
    Serial.begin(9600, SERIAL_8N1, SERIAL_RX_ONLY);
#else
    Serial.begin(115200);
#endif
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

    mSys.enableDebug();
    mSys.setup(&mTimestamp, mConfig->nrf.amplifierPower, mConfig->nrf.pinIrq, mConfig->nrf.pinCe, mConfig->nrf.pinCs, mConfig->nrf.pinSclk, mConfig->nrf.pinMosi, mConfig->nrf.pinMiso);

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

    mPayload.setup(this, &mSys, &mStat, mConfig->nrf.maxRetransPerPyld, &mTimestamp);
    mPayload.enableSerialDebug(mConfig->serial.debug);
    mPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1));

    mMiPayload.setup(this, &mSys, &mStat, mConfig->nrf.maxRetransPerPyld, &mTimestamp);
    mMiPayload.enableSerialDebug(mConfig->serial.debug);
    mMiPayload.addPayloadListener(std::bind(&app::payloadEventListener, this, std::placeholders::_1));

    // DBGPRINTLN("--- after payload");
    // DBGPRINTLN(String(ESP.getFreeHeap()));
    // DBGPRINTLN(String(ESP.getHeapFragmentation()));
    // DBGPRINTLN(String(ESP.getMaxFreeBlockSize()));

    if (!mSys.Radio.isChipConnected())
        DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));

    // when WiFi is in client mode, then enable mqtt broker
    #if !defined(AP_ONLY) && defined (AHOY_MQTT_SUPPORT)
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

    mApi.setup(this, &mSys, mWeb.getWebSrvPtr(), mConfig);

    // Plugins
    if (mConfig->plugin.display.type != 0)
        mDisplay.setup(&mConfig->plugin.display, &mSys, &mTimestamp, mVersion);

    mPubSerial.setup(mConfig, &mSys, &mTimestamp);

#ifdef AHOY_SML_OBIS_SUPPORT
    sml_setup (this, &mTimestamp);
#endif

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

    if (mSys.Radio.loop()) {
        while (!mSys.Radio.mBufCtrl.empty()) {
            packet_t *p = &mSys.Radio.mBufCtrl.front();

            if (mConfig->serial.debug) {
                DPRINT(DBG_INFO, F("RX "));
                DBGPRINT(String(p->len));
#ifdef undef
                DBGPRINT(F("B Ch"));
                DBGPRINT(String(p->ch));
                DBGPRINT(F(" | "));
                mSys.Radio.dumpBuf(p->packet, p->len);
#else
                DBGPRINTLN(" Bytes");
#endif
            }

            mStat.frmCnt++;

            Inverter<> *iv = mSys.findInverter(&p->packet[1]);
            if (NULL != iv) {
                if (IV_HM == iv->ivGen)
                    mPayload.add(iv, p);
                else
                    mMiPayload.add(iv, p);
            }
            mSys.Radio.mBufCtrl.pop();
            yield();
        }
        mPayload.process(true);
        mMiPayload.process(true);
    }
    mPayload.loop();
    mMiPayload.loop();

#ifdef AHOY_MQTT_SUPPORT
    if (mMqttEnabled) {
        mMqtt.loop();
    }
#endif

#ifdef AHOY_SML_OBIS_SUPPORT
    sml_loop ();
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
    regularTickers();  // reinstall regular tickers
    if (gotIp) {
        mInnerLoopCb = std::bind(&app::loopStandard, this);
        every(std::bind(&app::tickSend, this), mConfig->nrf.sendInterval, "tSend");

        mMqttReconnect = true;

        mSunrise = 0;  // needs to be set to 0, to reinstall sunrise and ivComm tickers!
        once(std::bind(&app::tickNtpUpdate, this), 2, "ntp2");
        if (WIFI_AP == WiFi.getMode()) {
#ifdef AHOY_MQTT_SUPPORT
            mMqttEnabled = false;
#endif
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
}

//-----------------------------------------------------------------------------
void app::tickNtpUpdate(void) {
    uint32_t nxtTrig = 5;  // default: check again in 5 sec
    bool isOK = mWifi.getNtpTime();
    if (isOK || mTimestamp != 0) {
#ifdef AHOY_MQTT_SUPPORT
        if (mMqttReconnect && mMqttEnabled) {
            mMqtt.tickerSecond();
            everySec(std::bind(&PubMqttType::tickerSecond, &mMqtt), "mqttS");
            everyMin(std::bind(&PubMqttType::tickerMinute, &mMqtt), "mqttM");
        }
#endif

        // only install schedulers once even if NTP wasn't successful in first loop
        if (mMqttReconnect) {  // @TODO: mMqttReconnect is variable which scope has changed
            if (mConfig->inst.rstValsNotAvail)
                everyMin(std::bind(&app::tickMinute, this), "tMin");

            uint32_t localTime = gTimezone.toLocal(mTimestamp);
            uint32_t midTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86400);  // next midnight local time
            onceAt(std::bind(&app::tickMidnight, this), midTrig, "midNi");

            mSys.cleanup_history();
#ifdef AHOY_SML_OBIS_SUPPORT
            sml_cleanup_history ();
#endif
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
#ifdef AHOY_MQTT_SUPPORT
    if (mMqttEnabled)
        tickSun();
#endif
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

#ifdef AHOY_MQTT_SUPPORT
//-----------------------------------------------------------------------------
void app::tickSun(void) {
    // only used and enabled by MQTT (see setup())
    if (!mMqtt.tickerSun(mSunrise, mSunset, mConfig->sun.offsetSec, mConfig->sun.disNightCom))
        once(std::bind(&app::tickSun, this), 1, "mqSun");  // MQTT not connected, retry
}
#endif

//-----------------------------------------------------------------------------
void app::tickComm(void) {
    if ((!mIVCommunicationOn) && (mConfig->inst.rstValsCommStop))
        once(std::bind(&app::tickZeroValues, this), mConfig->nrf.sendInterval, "tZero");

#ifdef AHOY_MQTT_SUPPORT
    if (mMqttEnabled) {
        if (!mMqtt.tickerComm(!mIVCommunicationOn))
            once(std::bind(&app::tickComm, this), 5, "mqCom");  // MQTT not connected, retry after 5s
    }
#endif
}

//-----------------------------------------------------------------------------
void app::tickZeroValues(void) {
    Inverter<> *iv;
    // set values to zero, except yields
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter

        mPayload.zeroInverterValues(iv);
    }
}

//-----------------------------------------------------------------------------
void app::tickMinute(void) {
    // only triggered if 'reset values on no avail is enabled'

    Inverter<> *iv;
    // set values to zero, except yields
    for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
        iv = mSys.getInverterByPos(id);
        if (NULL == iv)
            continue;  // skip to next inverter

        if (!iv->isAvailable(mTimestamp) && !iv->isProducing(mTimestamp) && iv->config->enabled)
            mPayload.zeroInverterValues(iv);
    }
}

//-----------------------------------------------------------------------------
void app::tickMidnight(void) {
    if (mConfig->inst.rstYieldMidNight) {
        // only if 'reset values at midnight is enabled'
        uint32_t localTime = gTimezone.toLocal(mTimestamp);
        uint32_t nxtTrig = gTimezone.toUTC(localTime - (localTime % 86400) + 86400);  // next midnight local time
        onceAt(std::bind(&app::tickMidnight, this), nxtTrig, "mid2");

        Inverter<> *iv;
        // set values to zero, except yield total
        for (uint8_t id = 0; id < mSys.getNumInverters(); id++) {
            iv = mSys.getInverterByPos(id);
            if (NULL == iv)
                continue;  // skip to next inverter

            mPayload.zeroInverterValues(iv);
            mPayload.zeroYieldDay(iv);
        }
#ifdef AHOY_MQTT_SUPPORT
        if (mMqttEnabled)
            mMqtt.tickerMidnight();
#endif
    }
    mSys.cleanup_history ();
#ifdef AHOY_SML_OBIS_SUPPORT
    sml_cleanup_history();
#endif
}

//-----------------------------------------------------------------------------
void app::tickSend(void) {
    if (!mSys.Radio.isChipConnected()) {
        DPRINTLN(DBG_WARN, F("NRF24 not connected!"));
        return;
    }
    if (mIVCommunicationOn) {
        if (!mSys.Radio.mBufCtrl.empty()) {
            if (mConfig->serial.debug) {
                DPRINT(DBG_INFO, F("recbuf not empty! #"));
                DBGPRINTLN(String(mSys.Radio.mBufCtrl.size()));
            }
        }

        int8_t maxLoop = MAX_NUM_INVERTERS;
        Inverter<> *iv = mSys.getInverterByPos(mSendLastIvId);
        do {
            mSendLastIvId = ((MAX_NUM_INVERTERS - 1) == mSendLastIvId) ? 0 : mSendLastIvId + 1;
            iv = mSys.getInverterByPos(mSendLastIvId);
        } while ((NULL == iv) && ((maxLoop--) > 0));

        if (NULL != iv) {
            if (iv->config->enabled) {
                    if (iv->ivGen == IV_HM)
                        mPayload.ivSend(iv);
                    else
                        mMiPayload.ivSend(iv);
            } else {
                DPRINTLN (DBG_INFO, "iv not enabled");
            }
        } else {
            DPRINTLN (DBG_INFO, "no inverter");
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

#ifdef AHOY_MQTT_SUPPORT
    mMqttEnabled = false;
#endif

    mSendLastIvId = 0;
    mShowRebootRequest = false;
    mIVCommunicationOn = true;
    mSavePending = false;
    mSaveReboot = false;

    memset(&mStat, 0, sizeof(statistics_t));
}

#ifdef AHOY_MQTT_SUPPORT
//-----------------------------------------------------------------------------
void app::mqttSubRxCb(JsonObject obj) {
    mApi.ctrlRequest(obj);
}
#endif

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

#ifdef AHOY_MQTT_SUPPORT
    if (mConfig->led.led1 != 0xff) {
        if (getMqttIsConnected()) {
            digitalWrite(mConfig->led.led1, led_on);
        } else {
            digitalWrite(mConfig->led.led1, led_off);
        }
    }
#endif
}

//-----------------------------------------------------------------------------
void app::check_hist_file (File file)
{
    if (file) {
        uint16_t exp_index = AHOY_MIN_PAC_SUN_HOUR * 60 / AHOY_PAC_INTERVAL, index;
        unsigned char data[4];

        while (file.read (data, sizeof (data)) == sizeof (data)) {
            index = data[0] + (data[1] << 8);
            if (index != exp_index) {
                DPRINTLN (DBG_WARN, "Unexpected " + String (index) + " <-> " + String (exp_index));
            }
            exp_index = index + 1;
        }
        file.close();
    }
}

//-----------------------------------------------------------------------------
void app::show_history (String path)
{
    Dir dir = LittleFS.openDir (path);

    DPRINTLN (DBG_INFO, "Enter Dir: " + path);
    while (dir.next()) {
        if (dir.isDirectory ()) {
            show_history (path + "/" + dir.fileName());
        } else {
            DPRINTLN (DBG_INFO, "file " + dir.fileName() +
                ", Size: " + String (dir.fileSize()));
            check_hist_file (dir.openFile ("r"));
        }
    }
    DPRINTLN (DBG_INFO, "Leave Dir: " + path);
}
