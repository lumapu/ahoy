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

class ahoywifi;
class web;

class app : public ah::Scheduler {
    public:
        app() : ah::Scheduler() {}
        ~app() {}

        void setup(uint32_t timeout);
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

        String getDateTimeStr(time_t t) {
            char str[20];
            if(0 == t)
                sprintf(str, "n/a");
            else
                sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", year(t), month(t), day(t), hour(t), minute(t), second(t));
            return String(str);
        }

        String getTimeStr(uint32_t offset = 0) {
            char str[10];
            if(0 == mUtcTimestamp)
                sprintf(str, "n/a");
            else
                sprintf(str, "%02d:%02d:%02d ", hour(mUtcTimestamp + offset), minute(mUtcTimestamp + offset), second(mUtcTimestamp + offset));
            return String(str);
        }

        inline uint32_t getUptime(void) {
            return mUptimeSecs;
        }

        inline uint32_t getTimestamp(void) {
            return mUtcTimestamp;
        }

        void setTimestamp(uint32_t newTime) {
            DPRINTLN(DBG_DEBUG, F("setTimestamp: ") + String(newTime));
            if(0 == newTime)
                mUpdateNtp = true;
            else
                mUtcTimestamp = newTime;
        }

        inline uint32_t getSunrise(void) {
            return mSunrise;
        }
        inline uint32_t getSunset(void) {
            return mSunset;
        }
        inline uint32_t getLatestSunTimestamp(void) {
            return mLatestSunTimestamp;
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

        void setupMqtt(void);

        void setupLed(void);
        void updateLed(void);

        void uptimeTick(void) {
            mUptimeSecs++;
            if (0 != mUtcTimestamp)
                mUtcTimestamp++;

            if (mShouldReboot) {
                DPRINTLN(DBG_INFO, F("Rebooting..."));
                ESP.restart();
            }

            if (mUpdateNtp) {
                mUpdateNtp = false;
                mUtcTimestamp = mWifi->getNtpTime();
                DPRINTLN(DBG_INFO, F("[NTP]: ") + getDateTimeStr(mUtcTimestamp) + F(" UTC"));
            }
        }

        void ntpUpdateTick(void) {
            if (!mWifi->getApActive())
                mUpdateNtp = true;
        }

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

        uint32_t mUptimeSecs;
        uint8_t mHeapStatCnt;

        uint32_t mUtcTimestamp;
        bool mUpdateNtp;

        bool mShowRebootRequest;

        ahoywifi *mWifi;
        web *mWeb;
        PayloadType mPayload;
        PubSerialType mPubSerial;

        char mVersion[12];
        settings mSettings;
        settings_t *mConfig;

        uint16_t mSendTicker;
        uint8_t mSendLastIvId;

        statistics_t mStat;

        // timer
        uint32_t mTicker;
        uint32_t mRxTicker;

        // mqtt
        PubMqttType mMqtt;
        bool mMqttActive;

        // sun
        int32_t mCalculatedTimezoneOffset;
        uint32_t mSunrise, mSunset;
        uint32_t mLatestSunTimestamp;
};

#endif /*__APP_H__*/
