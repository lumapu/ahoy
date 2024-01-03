//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __APP_H__
#define __APP_H__

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config/settings.h"
#include "defines.h"
#include "appInterface.h"
#include "hm/hmSystem.h"
#include "hm/hmRadio.h"
#if defined(ESP32)
#include "hms/hmsRadio.h"
#endif
#include "publisher/pubMqtt.h"
#include "publisher/pubSerial.h"
#include "utils/crc.h"
#include "utils/dbg.h"
#include "utils/scheduler.h"
#include "utils/syslog.h"
#include "web/RestApi.h"
#include "web/web.h"
#include "hm/Communication.h"
#if defined(ETHERNET)
    #include "eth/ahoyeth.h"
#else /* defined(ETHERNET) */
    #include "wifi/ahoywifi.h"
    #include "utils/improv.h"
#endif /* defined(ETHERNET) */

#include <RF24.h> // position is relevant since version 1.4.7 of this library

// convert degrees and radians for sun calculation
#define SIN(x) (sin(radians(x)))
#define COS(x) (cos(radians(x)))
#define ASIN(x) (degrees(asin(x)))
#define ACOS(x) (degrees(acos(x)))

typedef HmSystem<MAX_NUM_INVERTERS> HmSystemType;
#ifdef ESP32
#endif
typedef Web<HmSystemType> WebType;
typedef RestApi<HmSystemType> RestApiType;
typedef PubMqtt<HmSystemType> PubMqttType;
typedef PubSerial<HmSystemType> PubSerialType;

// PLUGINS
#if defined(PLUGIN_DISPLAY)
#include "plugins/Display/Display.h"
#include "plugins/Display/Display_data.h"

//typedef Display<HmSystemType> DisplayType;
typedef Display<HmSystemType, Radio> DisplayType;
#endif
#include "plugins/history.h"

class app : public IApp, public ah::Scheduler {
   public:
        app();
        ~app() {}

        void setup(void);
        void loop(void);
        void onNetwork(bool gotIp);
        void regularTickers(void);

        void handleIntr(void) {
            mNrfRadio.handleIntr();
        }
        void* getRadioObj(bool nrf) {
            if(nrf)
                return (void*)&mNrfRadio;
            else {
                #ifdef ESP32
                return (void*)&mCmtRadio;
                #else
                return NULL;
                #endif
            }
        }

        #ifdef ESP32
        void handleHmsIntr(void) {
            mCmtRadio.handleIntr();
        }
        #endif

        uint32_t getUptime() {
            return Scheduler::getUptime();
        }

        uint32_t getTimestamp() {
            return Scheduler::mTimestamp;
        }

        uint64_t getTimestampMs() {
            return ((uint64_t)Scheduler::mTimestamp * 1000) + (uint64_t)Scheduler::mTsMillis;
        }

        bool saveSettings(bool reboot) {
            mShowRebootRequest = true; // only message on index, no reboot
            mSavePending = true;
            mSaveReboot = reboot;
            if(reboot) {
                ah::Scheduler::resetTicker();
            }
            once(std::bind(&app::tickSave, this), 3, "save");
            return true;
        }

        void initInverter(uint8_t id) {
            mSys.addInverter(id, [this](Inverter<> *iv) {
                if((IV_MI == iv->ivGen) || (IV_HM == iv->ivGen))
                    iv->radio = &mNrfRadio;
                #if defined(ESP32)
                else if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen))
                    iv->radio = &mCmtRadio;
                #endif
            });
        }

        bool readSettings(const char *path) {
            return mSettings.readSettings(path);
        }

        bool eraseSettings(bool eraseWifi = false) {
            return mSettings.eraseSettings(eraseWifi);
        }

        bool getSavePending() {
            return mSavePending;
        }

        bool getLastSaveSucceed() {
            return mSettings.getLastSaveSucceed();
        }

        bool getShouldReboot() {
            return mSaveReboot;
        }

        #if !defined(ETHERNET)
        void scanAvailNetworks() {
            mWifi.scanAvailNetworks();
        }

        bool getAvailNetworks(JsonObject obj) {
            return mWifi.getAvailNetworks(obj);
        }

        #endif /* !defined(ETHERNET) */

        void setRebootFlag() {
            once(std::bind(&app::tickReboot, this), 3, "rboot");
        }

        const char *getVersion() {
            return mVersion;
        }

        uint32_t getSunrise() {
            return mSunrise;
        }

        uint32_t getSunset() {
            return mSunset;
        }

        bool getSettingsValid() {
            return mSettings.getValid();
        }

        bool getRebootRequestState() {
            return mShowRebootRequest;
        }

        void setMqttDiscoveryFlag() {
            once(std::bind(&PubMqttType::sendDiscoveryConfig, &mMqtt), 1, "disCf");
        }

        void setMqttPowerLimitAck(Inverter<> *iv) {
            mMqtt.setPowerLimitAck(iv);
        }

        bool getMqttIsConnected() {
            return mMqtt.isConnected();
        }

        uint32_t getMqttTxCnt() {
            return mMqtt.getTxCnt();
        }

        uint32_t getMqttRxCnt() {
            return mMqtt.getRxCnt();
        }

        bool getProtection(AsyncWebServerRequest *request) {
            return mWeb.isProtected(request);
        }

        bool getNrfEnabled(void) {
            return mConfig->nrf.enabled;
        }

        bool getCmtEnabled(void) {
            return mConfig->cmt.enabled;
        }

        uint8_t getNrfIrqPin(void) {
            return mConfig->nrf.pinIrq;
        }

        uint8_t getCmtIrqPin(void) {
            return mConfig->cmt.pinIrq;
        }

        uint32_t getTimezoneOffset() {
            return mApi.getTimezoneOffset();
        }

        void getSchedulerInfo(uint8_t *max) {
            getStat(max);
        }

        void getSchedulerNames(void) {
            printSchedulers();
        }

        void setTimestamp(uint32_t newTime) {
            DPRINT(DBG_DEBUG, F("setTimestamp: "));
            DBGPRINTLN(String(newTime));
            if(0 == newTime)
            {
                #if defined(ETHERNET)
                mEth.updateNtpTime();
                #else /* defined(ETHERNET) */
                mWifi.getNtpTime();
                #endif /* defined(ETHERNET) */
            }
            else
                Scheduler::setTimestamp(newTime);
        }

        TotalPowerHistory *getTotalPowerHistoryPtr() { return mTotalPowerHistory; };
        YieldDayHistory   *getYieldDayHistoryPtr()   { return mYieldDayHistory; };

       private:
        #define CHECK_AVAIL     true
        #define SKIP_YIELD_DAY  true

        void resetSystem(void);
        void zeroIvValues(bool checkAvail = false, bool skipYieldDay = true);

        void payloadEventListener(uint8_t cmd, Inverter<> *iv) {
            #if !defined(AP_ONLY)
            if (mMqttEnabled)
                mMqtt.payloadEventListener(cmd, iv);
            #endif
            #if defined(PLUGIN_DISPLAY)
            if(mConfig->plugin.display.type != 0)
               mDisplay.payloadEventListener(cmd);
            #endif
           updateLed();
        }

        void mqttSubRxCb(JsonObject obj);

        void setupLed();
        void updateLed();

        void tickReboot(void) {
            DPRINTLN(DBG_INFO, F("Rebooting..."));
            ah::Scheduler::resetTicker();
            WiFi.disconnect();
            delay(200);
            ESP.restart();
        }

        void tickSave(void) {
            if(!mSettings.saveSettings())
                mSaveReboot = false;
            mSavePending = false;

            if(mSaveReboot)
                setRebootFlag();
        }

        void tickNtpUpdate(void);
        #if defined(ETHERNET)
        void onNtpUpdate(bool gotTime);
        #endif /* defined(ETHERNET) */
        void updateNtp(void);

        void tickCalcSunrise(void);
        void tickIVCommunication(void);
        void tickSun(void);
        void tickComm(void);
        void tickSend(void);
        void tickMinute(void);
        void tickZeroValues(void);
        void tickMidnight(void);

        HmSystemType mSys;
        HmRadio<> mNrfRadio;
        Communication mCommunication;

        bool mShowRebootRequest;

        #if defined(ETHERNET)
        ahoyeth mEth;
        #else /* defined(ETHERNET) */
        ahoywifi mWifi;
        #endif /* defined(ETHERNET) */
        WebType mWeb;
        RestApiType mApi;
        #ifdef ENABLE_SYSLOG
        DbgSyslog mDbgSyslog;
        #endif
        //PayloadType mPayload;
        //MiPayloadType mMiPayload;
        PubSerialType mPubSerial;
        #if !defined(ETHERNET)
        //Improv mImprov;
        #endif
        #ifdef ESP32
        CmtRadio<> mCmtRadio;
        #endif

        char mVersion[12];
        settings mSettings;
        settings_t *mConfig;
        bool mSavePending;
        bool mSaveReboot;

        uint8_t mSendLastIvId;
        bool mSendFirst;

        bool mNetworkConnected;

        // mqtt
        PubMqttType mMqtt;
        bool mMqttReconnect;
        bool mMqttEnabled;

        // sun
        int32_t mCalculatedTimezoneOffset;
        uint32_t mSunrise, mSunset;

        // plugins
        #if defined(PLUGIN_DISPLAY)
        DisplayType mDisplay;
        DisplayData mDispData;
        #endif
        TotalPowerHistory *mTotalPowerHistory;
        YieldDayHistory   *mYieldDayHistory;

};

#endif /*__APP_H__*/
