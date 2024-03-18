//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_WIFI_AP_H__
#define __AHOY_WIFI_AP_H__

#include "../utils/dbg.h"
#include <Arduino.h>
#include "../config/settings.h"
#include "AhoyNetworkHelper.h"

class AhoyWifiAp {
    public:
        AhoyWifiAp() : mIp(192, 168, 4, 1) {}

        void setup(cfgSys_t *cfg) {
            mCfg = cfg;
        }

        void tickLoop() {
            if(mEnabled)
                mDns.processNextRequest();
        }

        void enable() {
            ah::welcome(mIp.toString(), String(F("Password: ") + String(mCfg->apPwd)));
            if('\0' == mCfg->deviceName[0])
                snprintf(mCfg->deviceName, DEVNAME_LEN, "%s", DEF_DEVICE_NAME);
            WiFi.hostname(mCfg->deviceName);

            #if defined(ETHERNET)
            WiFi.mode(WIFI_AP);
            #else
            WiFi.mode(WIFI_AP_STA);
            #endif
            WiFi.softAPConfig(mIp, mIp, IPAddress(255, 255, 255, 0));
            WiFi.softAP(WIFI_AP_SSID, mCfg->apPwd);

            mDns.start(53, "*", mIp);

            mEnabled = true;
            tickLoop();
        }

        void disable() {
            mDns.stop();
            WiFi.softAPdisconnect();
            #if defined(ETHERNET)
            WiFi.mode(WIFI_OFF);
            #else
            WiFi.mode(WIFI_STA);
            #endif

            mEnabled = false;
        }

        bool getEnable() const {
            return mEnabled;
        }

    private:
        cfgSys_t *mCfg = nullptr;
        DNSServer mDns;
        IPAddress mIp;
        bool mEnabled = false;
};

#endif /*__AHOY_WIFI_AP_H__*/
