//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __AHOYWIFI_H__
#define __AHOYWIFI_H__

#include "dbg.h"

// NTP
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DNSServer.h>

#include "defines.h"

#include "app.h"

class app;

class ahoywifi {
    public:
        ahoywifi(app *main, sysConfig_t *sysCfg, config_t *config);
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
        bool mStationWifiIsDef;
};

#endif /*__AHOYWIFI_H__*/
