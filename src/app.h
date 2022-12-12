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

#include "publisher/pubMqtt.h"
#include "publisher/pubSerial.h"


// convert degrees and radians for sun calculation
#define SIN(x) (sin(radians(x)))
#define COS(x) (cos(radians(x)))
#define ASIN(x) (degrees(asin(x)))
#define ACOS(x) (degrees(acos(x)))

typedef HmSystem<MAX_NUM_INVERTERS> HmSystemType;
typedef Payload<HmSystemType> PayloadType;
typedef PubMqtt<HmSystemType> PubMqttType;
typedef PubSerial<HmSystemType> PubSerialType;

// PLUGINS
#if defined(ENA_NOKIA) || defined(ENA_SSD1306)
    #include "plugins/MonochromeDisplay/MonochromeDisplay.h"
    typedef MonochromeDisplay<HmSystemType> MonoDisplayType;
#endif

class web;

class app : public ah::Scheduler {
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
        void scanAvailNetworks(void);
        void getAvailNetworks(JsonObject obj);

        void saveSettings(void) {
            mSettings.saveSettings();
        }

        bool eraseSettings(bool eraseWifi = false) {
            return mSettings.eraseSettings(eraseWifi);
        }

        uint8_t getIrqPin(void) {
            return mConfig->nrf.pinIrq;
        }

        uint64_t Serial2u64(const char *val) {
            char tmp[3];
            uint64_t ret = 0ULL;
            uint64_t u64;
            memset(tmp, 0, 3);
            for(uint8_t i = 0; i < 6; i++) {
                tmp[0] = val[i*2];
                tmp[1] = val[i*2 + 1];
                if((tmp[0] == '\0') || (tmp[1] == '\0'))
                    break;
                u64 = strtol(tmp, NULL, 16);
                ret |= (u64 << ((5-i) << 3));
            }
            return ret;
        }

        String getTimeStr(uint32_t offset = 0) {
            char str[10];
            if(0 == mTimestamp)
                sprintf(str, "n/a");
            else
                sprintf(str, "%02d:%02d:%02d ", hour(mTimestamp + offset), minute(mTimestamp + offset), second(mTimestamp + offset));
            return String(str);
        }

        void setTimestamp(uint32_t newTime) {
            DPRINTLN(DBG_DEBUG, F("setTimestamp: ") + String(newTime));
            if(0 == newTime)
                mUpdateNtp = true;
            else
                Scheduler::setTimestamp(newTime);
        }

        inline uint32_t getSunrise(void) {
            return mSunrise;
        }
        inline uint32_t getSunset(void) {
            return mSunset;
        }

        inline bool mqttIsConnected(void) { return mMqtt.isConnected(); }
        inline bool getSettingsValid(void) { return mSettings.getValid(); }
        inline bool getRebootRequestState(void) { return mShowRebootRequest; }
        inline uint32_t getMqttTxCnt(void) { return mMqtt.getTxCnt(); }

        HmSystemType *mSys;
        bool mShouldReboot;
        bool mFlagSendDiscoveryConfig;

    private:
        void resetSystem(void);

        void mqttSubRxCb(JsonObject obj);

        void setupLed(void);
        void updateLed(void);

        void tickSecond(void) {
            if (mShouldReboot) {
                DPRINTLN(DBG_INFO, F("Rebooting..."));
                ESP.restart();
            }

            if (mUpdateNtp) {
                mUpdateNtp = false;
                mWifi.getNtpTime();
            }
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

        bool mUpdateNtp;

        bool mShowRebootRequest;

        ahoywifi mWifi;
        web *mWeb;
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
