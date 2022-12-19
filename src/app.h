//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __APP_H__
#define __APP_H__


#include "utils/dbg.h"
#include <Arduino.h>
#include <RF24.h>
#include <RF24_config.h>
#include <ArduinoJson.h>

#include "appInterface.h"

#include "config/settings.h"
#include "defines.h"
#include "utils/crc.h"
#include "utils/ahoyTimer.h"
#include "utils/scheduler.h"

#include "hm/CircularBuffer.h"
#include "hm/hmSystem.h"
#include "hm/payload.h"
#include "wifi/ahoywifi.h"
#include "web/web.h"
#include "web/RestApi.h"

#include "publisher/pubMqtt.h"
#include "publisher/pubSerial.h"


// convert degrees and radians for sun calculation
#define SIN(x) (sin(radians(x)))
#define COS(x) (cos(radians(x)))
#define ASIN(x) (degrees(asin(x)))
#define ACOS(x) (degrees(acos(x)))

typedef HmSystem<MAX_NUM_INVERTERS> HmSystemType;
typedef Payload<HmSystemType> PayloadType;
typedef Web<HmSystemType> WebType;
typedef RestApi<HmSystemType> RestApiType;
typedef PubMqtt<HmSystemType> PubMqttType;
typedef PubSerial<HmSystemType> PubSerialType;

// PLUGINS
#if defined(ENA_NOKIA) || defined(ENA_SSD1306)
    #include "plugins/MonochromeDisplay/MonochromeDisplay.h"
    typedef MonochromeDisplay<HmSystemType> MonoDisplayType;
#endif


class app : public IApp, public ah::Scheduler {
    public:
        app();
        ~app() {}

        void setup(void);
        void loop(void);
        void handleIntr(void);
        void cbMqtt(char* topic, byte* payload, unsigned int length);
        void saveValues(void);
        void resetPayload(Inverter<>* iv);
        bool getWifiApActive(void);

        uint32_t getUptime() {
            return Scheduler::getUptime();
        }

        uint32_t getTimestamp() {
            return Scheduler::getTimestamp();
        }

        bool saveSettings() {
            mShowRebootRequest = true;
            return mSettings.saveSettings();
        }

        bool eraseSettings(bool eraseWifi = false) {
            return mSettings.eraseSettings(eraseWifi);
        }

        statistics_t *getStatistics() {
            return &mStat;
        }

        void scanAvailNetworks() {
            mWifi.scanAvailNetworks();
        }

        void getAvailNetworks(JsonObject obj) {
            mWifi.getAvailNetworks(obj);
        }

        void setRebootFlag() {
            once(std::bind(&app::tickReboot, this), 1);
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
            once(std::bind(&PubMqttType::sendDiscoveryConfig, &mMqtt), 1);
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

        uint8_t getIrqPin(void) {
            return mConfig->nrf.pinIrq;
        }

        String getTimeStr(uint32_t offset = 0) {
            char str[10];
            if(0 == mTimestamp)
                sprintf(str, "n/a");
            else
                sprintf(str, "%02d:%02d:%02d ", hour(mTimestamp + offset), minute(mTimestamp + offset), second(mTimestamp + offset));
            return String(str);
        }

        uint32_t getTimezoneOffset() {
            return mApi.getTimezoneOffset();
        }

        void setTimestamp(uint32_t newTime) {
            DPRINTLN(DBG_DEBUG, F("setTimestamp: ") + String(newTime));
            if(0 == newTime)
                mWifi.getNtpTime();
            else
                Scheduler::setTimestamp(newTime);
        }

        HmSystemType *mSys;

    private:
        void resetSystem(void);

        void payloadEventListener(uint8_t cmd) {
            #if !defined(AP_ONLY)
            mMqtt.payloadEventListener(cmd);
            #endif
            #if defined(ENA_NOKIA) || defined(ENA_SSD1306)
            mMonoDisplay.payloadEventListener(cmd);
            #endif
        }

        void mqttSubRxCb(JsonObject obj);

        void setupLed(void);
        void updateLed(void);

        void tickReboot(void) {
            DPRINTLN(DBG_INFO, F("Rebooting..."));
            ESP.restart();
        }

        void tickNtpUpdate(void);

        void tickCalcSunrise(void);
        void tickSend(void);

        void stats(void) {
            DPRINTLN(DBG_VERBOSE, F("main.h:stats"));
            #ifdef ESP8266
                uint32_t free;
                uint16_t max;
                uint8_t frag;
                ESP.getHeapStats(&free, &max, &frag);
            #elif defined(ESP32)
                uint32_t free;
                uint32_t max;
                uint8_t frag;
                free = ESP.getFreeHeap();
                max = ESP.getMaxAllocHeap();
                frag = 0;
            #endif
            DPRINT(DBG_VERBOSE, F("free: ") + String(free));
            DPRINT(DBG_VERBOSE, F(" - max: ") + String(max) + "%");
            DPRINTLN(DBG_VERBOSE, F(" - frag: ") + String(frag));
        }

        bool mShowRebootRequest;

        ahoywifi mWifi;
        WebType mWeb;
        RestApiType mApi;
        PayloadType mPayload;
        PubSerialType mPubSerial;

        char mVersion[12];
        settings mSettings;
        settings_t *mConfig;

        uint8_t mSendLastIvId;

        statistics_t mStat;

        // timer
        uint32_t mRxTicker;

        // mqtt
        PubMqttType mMqtt;
        bool mMqttActive;

        // sun
        int32_t mCalculatedTimezoneOffset;
        uint32_t mSunrise, mSunset;

        // plugins
        #if defined(ENA_NOKIA) || defined(ENA_SSD1306)
        MonoDisplayType mMonoDisplay;
        #endif
};

#endif /*__APP_H__*/
