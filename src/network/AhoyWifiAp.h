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

            if (WiFi.softAPgetStationNum() != mLast) {
                mLast = WiFi.softAPgetStationNum();
                if(mLast > 0)
                    DBGPRINTLN(F("AP client connected"));
            }
        }

        void enable() {
            if(mEnabled)
                return;

            ah::welcome(mIp.toString(), String(F("Password: ") + String(mCfg->apPwd)));

            #if defined(ETHERNET)
            WiFi.mode(WIFI_AP);
            #else
            WiFi.mode(WIFI_AP_STA);
            #endif
            WiFi.softAPConfig(mIp, mIp, IPAddress(255, 255, 255, 0));
            WiFi.softAP(WIFI_AP_SSID, mCfg->apPwd);

            mDns.setErrorReplyCode(DNSReplyCode::NoError);
            mDns.start(53, "*", mIp);

            mEnabled = true;
            tickLoop();
        }

        void disable() {
            if(!mEnabled)
                return;

            if(WiFi.softAPgetStationNum() > 0)
                return;

            mDns.stop();
            WiFi.softAPdisconnect();
            #if defined(ETHERNET)
            WiFi.mode(WIFI_OFF);
            #else
            WiFi.mode(WIFI_STA);
            #endif

            mEnabled = false;
        }

        bool isEnabled() const {
            return mEnabled;
        }

    private:
        cfgSys_t *mCfg = nullptr;
        DNSServer mDns;
        IPAddress mIp;
        bool mEnabled = false;
        uint8_t mLast = 0;
};

#endif /*__AHOY_WIFI_AP_H__*/
