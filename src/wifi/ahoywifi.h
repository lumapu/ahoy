//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __AHOYWIFI_H__
#define __AHOYWIFI_H__

#include "../utils/dbg.h"
#include <Arduino.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DNSServer.h>
#include "ESPAsyncWebServer.h"

#include "../config/settings.h"

class app;

class ahoywifi {
    public:
        ahoywifi(settings_t *config);
        ~ahoywifi() {}

        void setup(uint32_t timeout, bool settingValid);
        bool loop(void);
        void setupAp(const char *ssid, const char *pwd);
        bool setupStation(uint32_t timeout);
        bool getApActive(void);
        time_t getNtpTime(void);
        void scanAvailNetworks(void);
        void getAvailNetworks(JsonObject obj);

    private:
        void sendNTPpacket(IPAddress& address);

        settings_t *mConfig;

        DNSServer *mDns;
        WiFiUDP *mUdp; // for time server

        uint32_t mWifiStationTimeout;
        uint32_t mNextTryTs;
        uint32_t mApLastTick;
        bool mApActive;
        bool wifiWasEstablished;
        bool mStationWifiIsDef;
};

#endif /*__AHOYWIFI_H__*/
