//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __APP_H__
#define __APP_H__

#include "dbg.h"
#include "Arduino.h"


#include <RF24.h>
#include <RF24_config.h>
#include <ArduinoJson.h>

#include "eep.h"
#include "defines.h"
#include "crc.h"

#include "CircularBuffer.h"
#include "hmSystem.h"
#include "mqtt.h"
#include "ahoywifi.h"
#include "web.h"

// convert degrees and radians for sun calculation
#define SIN(x) (sin(radians(x)))
#define COS(x) (cos(radians(x)))
#define ASIN(x) (degrees(asin(x)))
#define ACOS(x) (degrees(acos(x)))

//  hier läst sich das Verhalten der app in Bezug auf MQTT
//  durch PER-Conpiler defines  anpassen
//
// #define __MQTT_TEST__                   // MQTT Interval wird auf 10 Sekunden verkürzt ( nur für testzwecke )
#define __MQTT_AFTER_RX__               // versendet die MQTT Daten sobald die WR daten Aufbereitet wurden  ( gehört eigentlich ins Setup )
// #define __MQTT_NO_DISCOVERCONFIG__      // das versenden der MQTTDiscoveryConfig abschalten  ( gehört eigentlich ins Setup )

typedef CircularBuffer<packet_t, PACKET_BUFFER_SIZE> BufferType;
typedef HmRadio<DEF_RF24_CE_PIN, DEF_RF24_CS_PIN, BufferType> RadioType;
typedef Inverter<float> InverterType;
typedef HmSystem<RadioType, BufferType, MAX_NUM_INVERTERS, InverterType> HmSystemType;


typedef struct {
    uint8_t txCmd;
    uint8_t txId;
    uint8_t invId;
    uint32_t ts;
    uint8_t data[MAX_PAYLOAD_ENTRIES][MAX_RF_PAYLOAD_SIZE];
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool complete;
    uint8_t maxPackId;
    uint8_t retransmits;
    bool requested;
} invPayload_t;

class ahoywifi;
class web;

class app {
    public:
        app();
        ~app() {}

        void setup(uint32_t timeout);
        void loop(void);
        void handleIntr(void);
        void cbMqtt(char* topic, byte* payload, unsigned int length);
        void saveValues(void);
        void resetPayload(Inverter<>* iv);
        bool getWifiApActive(void);

        uint8_t getIrqPin(void) {
            return mConfig.pinIrq;
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

        String getTimeStr(void) {
            char str[20];
            if(0 == mTimestamp)
                sprintf(str, "n/a");
            else
                sprintf(str, "%02d:%02d:%02d ", hour(mTimestamp), minute(mTimestamp), second(mTimestamp));
            return String(str);
        }

        inline uint32_t getUptime(void) {
            return mUptimeSecs;
        }

        inline uint32_t getTimestamp(void) {
            return mTimestamp;
        }

        inline void setTimestamp(uint32_t newTime) {
            DPRINTLN(DBG_DEBUG, F("setTimestamp: ") + String(newTime));
            if(0 == newTime)
                mUpdateNtp = true;
            else
            {
                mUtcTimestamp = newTime;
                mTimestamp = mUtcTimestamp + ((TIMEZONE + offsetDayLightSaving(mUtcTimestamp)) * 3600);
            }
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

        void eraseSettings(bool all = false) {
            //DPRINTLN(DBG_VERBOSE, F("main.h:eraseSettings"));
            uint8_t buf[64];
            uint16_t addr = (all) ? ADDR_START : ADDR_START_SETTINGS;
            uint16_t end;

            memset(buf, 0xff, 64);
            do {
                end = addr + 64;
                if(end > (ADDR_SETTINGS_CRC + 2))
                    end = (ADDR_SETTINGS_CRC + 2);
                DPRINTLN(DBG_DEBUG, F("erase: 0x") + String(addr, HEX) + " - 0x" + String(end, HEX));
                mEep->write(addr, buf, (end-addr));
                addr = end;
            } while(addr < (ADDR_SETTINGS_CRC + 2));
            mEep->commit();
        }

        inline bool checkTicker(uint32_t *ticker, uint32_t interval) {
            uint32_t mil = millis();
            if(mil >= *ticker) {
                *ticker = mil + interval;
                return true;
            }
            else if(mil < (*ticker - interval)) {
                *ticker = mil + interval;
                return true;
            }

            return false;
        }

        inline bool mqttIsConnected(void) { return mMqtt.isConnected(); }
        inline bool getSettingsValid(void) { return mSettingsValid; }
        inline bool getRebootRequestState(void) { return mShowRebootRequest; }


        HmSystemType *mSys;
        bool mShouldReboot;
        bool mFlagSendDiscoveryConfig;

    private:
        void resetSystem(void);
        void loadDefaultConfig(void);
        void loadEEpconfig(void);
        void setupMqtt(void);

        void sendMqttDiscoveryConfig(void);
        
        bool buildPayload(uint8_t id);
        void processPayload(bool retransmit);
        void processPayload(bool retransmit, uint8_t cmd);

        const char* getFieldDeviceClass(uint8_t fieldId);
        const char* getFieldStateClass(uint8_t fieldId);

        inline uint16_t buildEEpCrc(uint32_t start, uint32_t length) {
            DPRINTLN(DBG_VERBOSE, F("main.h:buildEEpCrc"));
            uint8_t buf[32];
            uint16_t crc = 0xffff;
            uint8_t len;

            while(length > 0) {
                len = (length < 32) ? length : 32;
                mEep->read(start, buf, len);
                crc = Ahoy::crc16(buf, len, crc);
                start += len;
                length -= len;
            }
            return crc;
        }

        void updateCrc(void) {
            DPRINTLN(DBG_VERBOSE, F("app::updateCrc"));
            uint16_t crc;

            crc = buildEEpCrc(ADDR_START, ADDR_WIFI_CRC);
            DPRINTLN(DBG_DEBUG, F("new Wifi CRC: ") + String(crc, HEX));
            mEep->write(ADDR_WIFI_CRC, crc);

            crc = buildEEpCrc(ADDR_START_SETTINGS, ((ADDR_NEXT) - (ADDR_START_SETTINGS)));
            DPRINTLN(DBG_DEBUG, F("new Settings CRC: ") + String(crc, HEX));
            mEep->write(ADDR_SETTINGS_CRC, crc);

            mEep->commit();
        }

        bool checkEEpCrc(uint32_t start, uint32_t length, uint32_t crcPos) {
            DPRINTLN(DBG_VERBOSE, F("main.h:checkEEpCrc"));
            DPRINTLN(DBG_DEBUG, F("start: ") + String(start) + F(", length: ") + String(length));
            uint16_t crcRd, crcCheck;
            crcCheck = buildEEpCrc(start, length);
            mEep->read(crcPos, &crcRd);
            DPRINTLN(DBG_DEBUG, "CRC RD: " + String(crcRd, HEX) + " CRC CALC: " + String(crcCheck, HEX));
            return (crcCheck == crcRd);
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

        uint8_t offsetDayLightSaving(uint32_t local_t);
        void calculateSunriseSunset(void);

        uint32_t mUptimeSecs;
        uint32_t mPrevMillis;
        uint8_t mHeapStatCnt;
        uint32_t mNtpRefreshTicker;
        uint32_t mNtpRefreshInterval;


        bool mWifiSettingsValid;
        bool mSettingsValid;

        eep *mEep;
        uint32_t mUtcTimestamp;
        uint32_t mTimestamp;
        bool mUpdateNtp;

        bool mShowRebootRequest;

        ahoywifi *mWifi;
        web *mWebInst;
        sysConfig_t mSysConfig;
        config_t mConfig;
        char mVersion[12];

        uint16_t mSendTicker;
        uint8_t mSendLastIvId;

        invPayload_t mPayload[MAX_NUM_INVERTERS];
        statistics_t mStat;
        uint8_t mLastPacketId;

        // timer
        uint32_t mTicker;
        uint32_t mRxTicker;

        // mqtt
        mqtt mMqtt;
        uint16_t mMqttTicker;
        uint16_t mMqttInterval;
        bool mMqttActive;
        bool mMqttConfigSendState[MAX_NUM_INVERTERS];

        // serial
        uint16_t mSerialTicker;

        // sun
        uint32_t mSunrise;
        uint32_t mSunset;
        uint32_t mLatestSunTimestamp;
};

#endif /*__APP_H__*/
