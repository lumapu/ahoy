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
#include "wifi.h"
#include "web.h"

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

const char* const wemosPins[] = {"D3 (GPIO0)", "TX (GPIO1)", "D4 (GPIO2)", "RX (GPIO3)",
                                "D2 (GPIO4)", "D1 (GPIO5)", "GPIO6", "GPIO7", "GPIO8",
                                "GPIO9", "GPIO10", "GPIO11", "D6 (GPIO12)", "D7 (GPIO13)",
                                "D5 (GPIO14)", "D8 (GPIO15)", "D0 (GPIO16 - no IRQ!)"};
const char* const pinNames[] = {"CS", "CE", "IRQ"};
const char* const pinArgNames[] = {"pinCs", "pinCe", "pinIrq"};


typedef struct {
    uint8_t invId;
    uint32_t ts;
    uint8_t data[MAX_PAYLOAD_ENTRIES][MAX_RF_PAYLOAD_SIZE];
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool complete;
    uint8_t maxPackId;
    uint8_t retransmits;
    bool requested;
} invPayload_t;


class wifi;
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
        String getStatistics(void);
        String getLiveData(void);
        String getJson(void);
        bool getWifiApActive(void);

        uint8_t getIrqPin(void) {
            return mConfig.pinIrq;
        }

        uint64_t Serial2u64(const char *val) {
            char tmp[3] = {0};
            uint64_t ret = 0ULL;
            uint64_t u64;
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
            char str[20] = {0};
            if(0 == t)
                sprintf(str, "n/a");
            else
                sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", year(t), month(t), day(t), hour(t), minute(t), second(t));
            return String(str);
        }

        inline uint32_t getUptime(void) {
            return mUptimeSecs;
        }

        inline uint32_t getTimestamp(void) {
            return mTimestamp;
        }

        void eraseSettings(bool all = false) {
            //DPRINTLN(DBG_VERBOSE, F("main.h:eraseSettings"));
            uint8_t buf[64] = {0};
            uint16_t addr = (all) ? ADDR_START : ADDR_START_SETTINGS;
            uint16_t end;
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

        HmSystemType *mSys;

    private:
        void resetSystem(void);
        void loadDefaultConfig(void);
        void loadEEpconfig(void);
        void setupMqtt(void);
        
        bool buildPayload(uint8_t id);
        void processPayload(bool retransmit);
        void processPayload(bool retransmit, uint8_t cmd);

        void sendMqttDiscoveryConfig(void);
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
                crc = Hoymiles::crc16(buf, len, crc);
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
            uint32_t free;
            uint16_t max;
            uint8_t frag;
            ESP.getHeapStats(&free, &max, &frag);
            DPRINT(DBG_VERBOSE, F("free: ") + String(free));
            DPRINT(DBG_VERBOSE, F(" - max: ") + String(max) + "%");
            DPRINTLN(DBG_VERBOSE, F(" - frag: ") + String(frag));
        }


        uint32_t mUptimeTicker;
        uint16_t mUptimeInterval;
        uint32_t mUptimeSecs;
        uint8_t mHeapStatCnt;


        bool mWifiSettingsValid;
        bool mSettingsValid;

        eep *mEep;
        uint32_t mTimestamp;

        bool mShowRebootRequest;

        wifi *mWifi;
        web *mWebInst;
        sysConfig_t mSysConfig;
        config_t mConfig;
        char mVersion[12];

        uint16_t mSendTicker;
        uint8_t mSendLastIvId;

        invPayload_t mPayload[MAX_NUM_INVERTERS];
        uint32_t mRxFailed;
        uint32_t mRxSuccess;
        uint32_t mFrameCnt;
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
};

#endif /*__APP_H__*/
