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
#include "hm/NrfRadio.h"
#if defined(ESP32)
#include "hms/CmtRadio.h"
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
#include "plugins/MaxPower.h"
#if defined(ENABLE_HISTORY)
#include "plugins/history.h"
#endif /*ENABLE_HISTORY*/
#include "web/web.h"
#include "hm/Communication.h"
#if defined(ETHERNET)
    #include "network/AhoyEthernet.h"
#else /* defined(ETHERNET) */
    #if defined(ESP32)
        #include "network/AhoyWifiEsp32.h"
    #else
        #include "network/AhoyWifiEsp8266.h"
    #endif
#endif /* defined(ETHERNET) */
#include "utils/improv.h"

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
        void onNetwork(bool connected);
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
                if((IV_MI == iv->ivGen) || (IV_HM == iv->ivGen) || (IV_HERF == iv->ivGen))
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

        bool getAvailNetworks(JsonObject obj) override {
            return mNetwork->getAvailNetworks(obj, this);
        }

        void setupStation(void) override {
            mNetwork->begin();
        }

        bool getWasInCh12to14(void) const override {
            #if defined(ESP8266)
            return mNetwork->getWasInCh12to14();
            #else
            return false;
            #endif
        }

        String getIp(void) override {
            return mNetwork->getIp();
        }

        String getMac(void) override {
            return mNetwork->getMac();
        }

        bool isApActive(void) override {
            return mNetwork->isApActive();
        }

        bool isNetworkConnected() override {
            return mNetwork->isConnected();
        }

        void setRebootFlag() override {
            once(std::bind(&app::tickReboot, this), 3, "rboot");
        }

        const char *getVersion() override {
            return mVersion;
        }

        const char *getVersionModules() override {
            return mVersionModules;
        }

        void addOnce(ah::scdCb c, uint32_t timeout, const char *name) override {
            once(c, timeout, name);
        }

        uint32_t getSunrise() override {
            return mSunrise;
        }

        uint32_t getSunset() override {
            return mSunset;
        }

        bool getSettingsValid() override {
            return mConfig->valid;
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

        #if defined(ETHERNET)
        bool isWiredConnection() override {
            return mNetwork->isWiredConnection();
        }
        #endif

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

        bool cmtSearch(uint8_t id, uint8_t toCh) override {
            #if defined(ESP32)
            Inverter<> *iv;

            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                iv = mSys.getInverterByPos(i, true);
                if(nullptr != iv) {
                    if(i == id)
                        break;
                    else
                        iv = nullptr;
                }
            }

            if(nullptr != iv) {
                mCmtRadio.catchInverter(iv, toCh);
                return true;
            }
            #endif

            return false;
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
                mNetwork->updateNtpTime();
            else {
                Scheduler::setTimestamp(newTime);
                onNtpUpdate(false);
            }
        }

        float getTotalMaxPower(void) override {
            return mMaxPower.getTotalMaxPower();
        }

        uint16_t getHistoryValue(uint8_t type, uint16_t i) override {
            #if defined(ENABLE_HISTORY)
                return mHistory.valueAt((HistoryStorageType)type, i);
            #else
                return 0;
            #endif
        }

        uint32_t getHistoryPeriod(uint8_t type) override {
            #if defined(ENABLE_HISTORY)
                return mHistory.getPeriod((HistoryStorageType)type);
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
            mMaxPower.payloadEvent(cmd, iv);
            #if defined(ENABLE_MQTT)
                if (mMqttEnabled)
                    mMqtt.payloadEventListener(cmd, iv);
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

        void onNtpUpdate(uint32_t utcTimestamp);

        void triggerTickSend(uint8_t id) override {
            once([this, id]() {
                sendIv(mSys.getInverterByPos(id));
            }, 0, "devct");
        }

        void tickCalcSunrise(void);
        void tickIVCommunication(void);
        void tickSun(void);
        void tickSunrise(void);
        void tickComm(void);
        void tickSend(void);
        bool sendIv(Inverter<> *iv);
        void tickMinute(void);
        void tickZeroValues(void);
        void tickMidnight(void);
        void notAvailChanged(void);

        HmSystemType mSys;
        NrfRadio<> mNrfRadio;
        Communication mCommunication;

        bool mShowRebootRequest = false;

        AhoyNetwork *mNetwork = nullptr;
        WebType mWeb;
        RestApiType mApi;
        Protection *mProtection = nullptr;
        #ifdef ENABLE_SYSLOG
        DbgSyslog mDbgSyslog;
        #endif

        PubSerialType mPubSerial;
        //Improv mImprov;
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
        bool mAllIvNotAvail = false;

        bool mNetworkConnected = false;

        #if defined(ENABLE_MQTT)
        PubMqttType mMqtt;
        #endif
        bool mMqttEnabled = false;

        // sun
        int32_t mCalculatedTimezoneOffset = 0;
        uint32_t mSunrise = 0, mSunset = 0;

        // plugins
        MaxPower<float> mMaxPower;
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
