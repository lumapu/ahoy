//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __APP_H__
#define __APP_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#if defined(ESP32)
#include <esp_task_wdt.h>
#define WDT_TIMEOUT_SECONDS 8   // Watchdog Timeout 8s
#endif

#include "config/settings.h"
#include "defines.h"
#include "appInterface.h"
#include "hm/hmSystem.h"
#include "hm/hmRadio.h"
#if defined(ESP32)
#include "hms/hmsRadio.h"
#endif
#if defined(ENABLE_MQTT)
#include "publisher/pubMqtt.h"
#endif /*ENABLE_MQTT*/
#include "publisher/pubSerial.h"
#include "utils/crc.h"
#include "utils/dbg.h"
#include "utils/scheduler.h"
#include "utils/syslog.h"
#include "web/RestApi.h"
#include "web/Protection.h"
#if defined(ENABLE_HISTORY)
#include "plugins/history.h"
#endif /*ENABLE_HISTORY*/
#include "web/web.h"
#include "hm/Communication.h"
#if defined(ETHERNET)
    #include "eth/ahoyeth.h"
#else /* defined(ETHERNET) */
    #include "wifi/ahoywifi.h"
    #include "utils/improv.h"
#endif /* defined(ETHERNET) */

#if defined(ENABLE_SIMULATOR)
    #include "hm/simulator.h"
#endif /*ENABLE_SIMULATOR*/

#include <RF24.h> // position is relevant since version 1.4.7 of this library


// convert degrees and radians for sun calculation
#define SIN(x) (sin(radians(x)))
#define COS(x) (cos(radians(x)))
#define ASIN(x) (degrees(asin(x)))
#define ACOS(x) (degrees(acos(x)))

typedef HmSystem<MAX_NUM_INVERTERS> HmSystemType;
typedef Web<HmSystemType> WebType;
typedef RestApi<HmSystemType> RestApiType;
#if defined(ENABLE_MQTT)
typedef PubMqtt<HmSystemType> PubMqttType;
#endif /*ENABLE_MQTT*/
typedef PubSerial<HmSystemType> PubSerialType;
#if defined(ENABLE_HISTORY)
typedef HistoryData<HmSystemType> HistoryType;
#endif /*ENABLE_HISTORY*/
#if defined (ENABLE_SIMULATOR)
typedef Simulator<HmSystemType> SimulatorType;
#endif /*ENABLE_SIMULATOR*/

// PLUGINS
#if defined(PLUGIN_DISPLAY)
#include "plugins/Display/Display.h"
#include "plugins/Display/Display_data.h"
typedef Display<HmSystemType, Radio> DisplayType;
#endif

class app : public IApp, public ah::Scheduler {
   public:
        app();
        ~app() {}

        void setup(void);
        void loop(void) override;
        void onNetwork(bool gotIp);
        void regularTickers(void);

        void handleIntr(void) {
            mNrfRadio.handleIntr();
        }
        void* getRadioObj(bool nrf) override {
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

        uint32_t getUptime() override {
            return Scheduler::getUptime();
        }

        uint32_t getTimestamp() override {
            return Scheduler::mTimestamp;
        }

        uint64_t getTimestampMs() override {
            return ((uint64_t)Scheduler::mTimestamp * 1000) + ((uint64_t)millis() - (uint64_t)Scheduler::mTsMillis) % 1000;
        }

        bool saveSettings(bool reboot) override {
            mShowRebootRequest = true; // only message on index, no reboot
            mSavePending = true;
            mSaveReboot = reboot;
            if(reboot) {
                ah::Scheduler::resetTicker();
            }
            once(std::bind(&app::tickSave, this), 3, "save");
            return true;
        }

        void initInverter(uint8_t id) override {
            mSys.addInverter(id, [this](Inverter<> *iv) {
                if((IV_MI == iv->ivGen) || (IV_HM == iv->ivGen))
                    iv->radio = &mNrfRadio;
                #if defined(ESP32)
                else if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen))
                    iv->radio = &mCmtRadio;
                #endif
            });
        }

        bool readSettings(const char *path) override {
            return mSettings.readSettings(path);
        }

        bool eraseSettings(bool eraseWifi = false) {
            return mSettings.eraseSettings(eraseWifi);
        }

        bool getSavePending() override {
            return mSavePending;
        }

        bool getLastSaveSucceed() override {
            return mSettings.getLastSaveSucceed();
        }

        bool getShouldReboot() override {
            return mSaveReboot;
        }

        #if !defined(ETHERNET)
        void scanAvailNetworks() override {
            mWifi.scanAvailNetworks();
        }

        bool getAvailNetworks(JsonObject obj) override {
            return mWifi.getAvailNetworks(obj);
        }

        void setupStation(void) override {
            mWifi.setupStation();
        }

        void setStopApAllowedMode(bool allowed) override {
            mWifi.setStopApAllowedMode(allowed);
        }

        String getStationIp(void) override {
            return mWifi.getStationIp();
        }

        bool getWasInCh12to14(void) const override {
            return mWifi.getWasInCh12to14();
        }

        #endif /* !defined(ETHERNET) */

        void setRebootFlag() override {
            once(std::bind(&app::tickReboot, this), 3, "rboot");
        }

        const char *getVersion() override {
            return mVersion;
        }

        const char *getVersionModules() override {
            return mVersionModules;
        }

        uint32_t getSunrise() override {
            return mSunrise;
        }

        uint32_t getSunset() override {
            return mSunset;
        }

        bool getSettingsValid() override {
            return mSettings.getValid();
        }

        bool getRebootRequestState() override {
            return mShowRebootRequest;
        }

        void setMqttDiscoveryFlag() override {
            #if defined(ENABLE_MQTT)
            once(std::bind(&PubMqttType::sendDiscoveryConfig, &mMqtt), 1, "disCf");
            #endif
        }

        bool getMqttIsConnected() override {
            #if defined(ENABLE_MQTT)
                return mMqtt.isConnected();
            #else
                return false;
            #endif
        }

        uint32_t getMqttTxCnt() override {
            #if defined(ENABLE_MQTT)
                return mMqtt.getTxCnt();
            #else
                return 0;
            #endif
        }

        uint32_t getMqttRxCnt() override {
            #if defined(ENABLE_MQTT)
                return mMqtt.getRxCnt();
            #else
                return 0;
            #endif
        }

        void lock(bool fromWeb) override {
            mProtection->lock(fromWeb);
        }

        char *unlock(const char *clientIp, bool loginFromWeb) override {
            return mProtection->unlock(clientIp, loginFromWeb);
        }

        void resetLockTimeout(void) override {
            mProtection->resetLockTimeout();
        }

        bool isProtected(const char *clientIp, const char *token, bool askedFromWeb) const override {
            return mProtection->isProtected(clientIp, token, askedFromWeb);
        }

        bool getNrfEnabled(void) override {
            return mConfig->nrf.enabled;
        }

        bool getCmtEnabled(void) override {
            return mConfig->cmt.enabled;
        }

        uint8_t getNrfIrqPin(void) {
            return mConfig->nrf.pinIrq;
        }

        uint8_t getCmtIrqPin(void) {
            return mConfig->cmt.pinIrq;
        }

        uint32_t getTimezoneOffset() override {
            return mApi.getTimezoneOffset();
        }

        void getSchedulerInfo(uint8_t *max) override {
            getStat(max);
        }

        void getSchedulerNames(void) override {
            printSchedulers();
        }

        void setTimestamp(uint32_t newTime) override {
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

        uint16_t getHistoryValue(uint8_t type, uint16_t i) override {
            #if defined(ENABLE_HISTORY)
                return mHistory.valueAt((HistoryStorageType)type, i);
            #else
                return 0;
            #endif
        }

        uint32_t getHistoryPeriode(uint8_t type) override {
            #if defined(ENABLE_HISTORY)
                return mHistory.getPeriode((HistoryStorageType)type);
            #else
                return 0;
            #endif
        }

        uint16_t getHistoryMaxDay() override {
            #if defined(ENABLE_HISTORY)
                return mHistory.getMaximumDay();
            #else
                return 0;
            #endif
        }

        uint32_t getHistoryLastValueTs(uint8_t type) override {
            #if defined(ENABLE_HISTORY)
                return mHistory.getLastValueTs((HistoryStorageType)type);
            #else
                return 0;
            #endif
        }
        #if defined(ENABLE_HISTORY_LOAD_DATA)
        void addValueToHistory(uint8_t historyType, uint8_t valueType, uint32_t value) override {
            #if defined(ENABLE_HISTORY)
                return mHistory.addValue((HistoryStorageType)historyType, valueType, value);
            #endif
        }
        #endif

    private:
        #define CHECK_AVAIL     true
        #define SKIP_YIELD_DAY  true

        void resetSystem(void);
        void zeroIvValues(bool checkAvail = false, bool skipYieldDay = true);

        void payloadEventListener(uint8_t cmd, Inverter<> *iv) {
            #if !defined(AP_ONLY)
            #if defined(ENABLE_MQTT)
                if (mMqttEnabled)
                    mMqtt.payloadEventListener(cmd, iv);
            #endif /*ENABLE_MQTT*/
            #endif
            #if defined(PLUGIN_DISPLAY)
            if(DISP_TYPE_T0_NONE != mConfig->plugin.display.type)
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
        bool mNtpReceived = false;
        #endif /* defined(ETHERNET) */
        void updateNtp(void);

        void triggerTickSend() override {
            once(std::bind(&app::tickSend, this), 0, "tSend");
        }

        void tickCalcSunrise(void);
        void tickIVCommunication(void);
        void tickSun(void);
        void tickSunrise(void);
        void tickComm(void);
        void tickSend(void);
        void tickMinute(void);
        void tickZeroValues(void);
        void tickMidnight(void);
        void notAvailChanged(void);

        HmSystemType mSys;
        HmRadio<> mNrfRadio;
        Communication mCommunication;

        bool mShowRebootRequest = false;

        #if defined(ETHERNET)
        ahoyeth mEth;
        #else /* defined(ETHERNET) */
        ahoywifi mWifi;
        #endif /* defined(ETHERNET) */
        WebType mWeb;
        RestApiType mApi;
        Protection *mProtection = nullptr;
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
        char mVersionModules[12];
        settings mSettings;
        settings_t *mConfig = nullptr;
        bool mSavePending = false;
        bool mSaveReboot = false;

        uint8_t mSendLastIvId = 0;
        bool mSendFirst = false;
        bool mAllIvNotAvail = false;

        bool mNetworkConnected = false;

        // mqtt
        #if defined(ENABLE_MQTT)
        PubMqttType mMqtt;
        #endif /*ENABLE_MQTT*/
        bool mMqttReconnect = false;
        bool mMqttEnabled = false;

        // sun
        int32_t mCalculatedTimezoneOffset = 0;
        uint32_t mSunrise = 0, mSunset = 0;

        // plugins
        #if defined(PLUGIN_DISPLAY)
        DisplayType mDisplay;
        DisplayData mDispData;
        #endif
        #if defined(ENABLE_HISTORY)
        HistoryType mHistory;
        #endif /*ENABLE_HISTORY*/

        #if defined(ENABLE_SIMULATOR)
        SimulatorType mSimulator;
        #endif /*ENABLE_SIMULATOR*/
};

#endif /*__APP_H__*/
