//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __WIFI_H__
#define __WIFI_H__

#include "dbg.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// NTP
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DNSServer.h>

#include "defines.h"

#include "app.h"

class app;

class wifi {
    public:
        wifi(app *main, sysConfig_t *sysCfg, config_t *config);
        ~wifi() {}

        void setup(uint32_t timeout, bool settingValid);
        bool loop(void);
        void setupAp(const char *ssid, const char *pwd);
        bool setupStation(uint32_t timeout);
        bool getApActive(void);
        time_t getNtpTime(void);

    private:
        void sendNTPpacket(IPAddress& address);
        time_t offsetDayLightSaving (uint32_t local_t);


        config_t *mConfig;
        sysConfig_t *mSysCfg;
        app *mMain;

        DNSServer *mDns;
        WiFiUDP *mUdp; // for time server

        uint32_t mWifiStationTimeout;
        uint32_t mNextTryTs;
        uint32_t mApLastTick;
        bool mApActive;
        bool wifiWasEstablished;
};

#endif /*__WIFI_H__*/
