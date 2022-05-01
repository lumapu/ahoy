#ifndef __MAIN_H__
#define __MAIN_H__

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
        virtual void setup(const char *ssid, const char *pwd, uint32_t timeout);
        virtual void loop();
        String getDateTimeStr (time_t t);


    protected:
        void showReboot(void);
        virtual void saveValues(bool webSend);
        virtual void updateCrc(void);

        inline uint16_t buildEEpCrc(uint32_t start, uint32_t length) {
            uint8_t buf[length];
            mEep->read(start, buf, length);
            return crc16(buf, length);
        }

        bool checkEEpCrc(uint32_t start, uint32_t length, uint32_t crcPos) {
            uint16_t crcRd, crcCheck;
            crcCheck = buildEEpCrc(start, length);
            mEep->read(crcPos, &crcRd);
            return (crcCheck == crcRd);
        }

        void eraseSettings(void) {
            uint8_t buf[64] = {0};
            uint16_t addr = ADDR_START_SETTINGS, end;
            do {
                end = addr += 64;
                if(end > (ADDR_SETTINGS_CRC + 2))
                    end = (ADDR_SETTINGS_CRC + 2 - addr);
                mEep->write(ADDR_START_SETTINGS, buf, (ADDR_NEXT-ADDR_START_SETTINGS));
            } while(addr < ADDR_START_SETTINGS);
        }

        inline bool checkTicker(uint32_t *ticker, uint16_t *interval) {
            uint32_t mil = millis();
            if(mil >= *ticker) {
                *ticker = mil + *interval;
                return true;
            }
            else if(mil < (*ticker - *interval)) {
                *ticker = mil + *interval;
                return true;
            }

            return false;
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

        time_t getNtpTime(void);
        void sendNTPpacket(IPAddress& address);
        time_t offsetDayLightSaving (uint32_t local_t);

        uint32_t mUptimeTicker;
        uint16_t mUptimeInterval;
        uint32_t mUptimeSecs;

        DNSServer *mDns;
        ESP8266HTTPUpdateServer *mUpdater;

        WiFiUDP *mUdp; // for time server
};

#endif /*__MAIN_H__*/
