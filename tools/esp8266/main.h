//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __MAIN_H__
#define __MAIN_H__

#include "dbg.h"
#include "Arduino.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include <ESP8266HTTPUpdateServer.h>

// NTP
#include <WiFiUdp.h>
#include <TimeLib.h>

#include "eep.h"
#include "defines.h"
#include "crc.h"


const byte mDnsPort = 53;

/* TIMESERVER CONFIG */
#define TIMESERVER_NAME     "pool.ntp.org"
#define TIME_LOCAL_PORT     8888
#define NTP_PACKET_SIZE     48
#define TIMEZONE            1 // Central European time +1

class Main {
    public:
        Main(void);
        virtual void setup(uint32_t timeout);
        virtual void loop();
        String getDateTimeStr (time_t t);


    protected:
        void showReboot(void);
        virtual void saveValues(bool webSend);
        virtual void updateCrc(void);

        inline uint16_t buildEEpCrc(uint32_t start, uint32_t length) {
            DPRINTLN(DBG_VERBOSE, F("main.h:buildEEpCrc"));
            uint8_t buf[32];
            uint16_t crc = 0xffff;
            uint8_t len;

            while(length > 0) {
                len = (length < 32) ? length : 32;
                mEep->read(start, buf, len);
                crc = crc16(buf, len, crc);
                start += len;
                length -= len;
            }
            return crc;
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
            //DPRINTLN(DBG_VERBOSE, F("c"));
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

        char mStationSsid[SSID_LEN];
        char mStationPwd[PWD_LEN];
        bool mWifiSettingsValid;
        bool mSettingsValid;
        bool mApActive;
        ESP8266WebServer *mWeb;
        char mVersion[9];
        char mDeviceName[DEVNAME_LEN];
        eep *mEep;
        uint32_t mTimestamp;
        uint32_t mLimit;
        uint32_t mNextTryTs;
        uint32_t mApLastTick;

    private:
        bool getConfig(void);
        void setupAp(const char *ssid, const char *pwd);
        bool setupStation(uint32_t timeout);

        void showNotFound(void);
        virtual void showSetup(void);
        virtual void showSave(void);
        void showUptime(void);
        void showTime(void);
        void showCss(void);
        void showFactoryRst(void);

        time_t getNtpTime(void);
        void sendNTPpacket(IPAddress& address);
        time_t offsetDayLightSaving (uint32_t local_t);

        uint32_t mUptimeTicker;
        uint16_t mUptimeInterval;
        uint32_t mUptimeSecs;
        uint8_t mHeapStatCnt;

        DNSServer *mDns;
        ESP8266HTTPUpdateServer *mUpdater;

        WiFiUDP *mUdp; // for time server
};

#endif /*__MAIN_H__*/
