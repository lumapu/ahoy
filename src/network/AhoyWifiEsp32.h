//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_WIFI_ESP32_H__
#define __AHOY_WIFI_ESP32_H__

#if defined(ESP32) && !defined(ETHERNET)
#include <functional>
#include <AsyncUDP.h>
#include "AhoyNetwork.h"
#include "ESPAsyncWebServer.h"

class AhoyWifi : public AhoyNetwork {
    public:
        void begin() override {
            mAp.enable();

            // static IP
            setupIp([this](IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns1, IPAddress dns2) -> bool {
                return WiFi.config(ip, gateway, mask, dns1, dns2);
            });

            WiFi.setHostname(mConfig->sys.deviceName);
            #if !defined(AP_ONLY)
                WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
                WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
                WiFi.begin(mConfig->sys.stationSsid, mConfig->sys.stationPwd, WIFI_ALL_CHANNEL_SCAN);

                DBGPRINT(F("connect to network '"));
                DBGPRINT(mConfig->sys.stationSsid);
                DBGPRINTLN(F("'"));
            #endif
        }

        void tickNetworkLoop() override {
            if(mAp.isEnabled())
                mAp.tickLoop();

            switch(mStatus) {
                case NetworkState::DISCONNECTED:
                    if(mConnected) {
                        mConnected = false;
                        mOnNetworkCB(false);
                        mAp.enable();
                        MDNS.end();
                    }
                    break;

                case NetworkState::CONNECTED:
                    break;

                case NetworkState::GOT_IP:
                    if(mAp.isEnabled())
                        mAp.disable();

                    if(!mConnected) {
                        mConnected = true;
                        ah::welcome(WiFi.localIP().toString(), F("Station"));
                        MDNS.begin(mConfig->sys.deviceName);
                        //MDNS.addServiceTxt("http", "tcp", "path", "/");
                        mOnNetworkCB(true);
                    }
                    break;
            }
        }

        String getIp(void) override {
            return WiFi.localIP().toString();
        }
};

#endif /*ESP32 & !ETHERNET*/
#endif /*__AHOY_WIFI_ESP32_H__*/
