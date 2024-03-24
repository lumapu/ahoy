//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_WIFI_ESP32_H__
#define __AHOY_WIFI_ESP32_H__

#if defined(ESP32) && !defined(ETHERNET)
#include <functional>
#include <AsyncUDP.h>
#include <Wifi.h>
#include "AhoyNetwork.h"

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

                DBGPRINT(F("connect to network '")); Serial.flush();
                DBGPRINT(mConfig->sys.stationSsid);
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
                    }

                    if (WiFi.softAPgetStationNum() > 0) {
                        DBGPRINTLN(F("AP client connected"));
                    }
                    break;

                case NetworkState::CONNECTED:
                    break;

                case NetworkState::GOT_IP:
                    if(!mConnected) {
                        mAp.disable();
                        mConnected = true;
                        ah::welcome(WiFi.localIP().toString(), F("Station"));
                        MDNS.begin(mConfig->sys.deviceName);
                        mOnNetworkCB(true);
                    }
                    break;
            }
        }

        String getIp(void) override {
            return WiFi.localIP().toString();
        }

        void scanAvailNetworks(void) override {
            if(!mScanActive) {
                mScanActive = true;
                WiFi.scanNetworks(true);
            }
        }

        bool getAvailNetworks(JsonObject obj) override {
            JsonArray nets = obj.createNestedArray(F("networks"));

            int n = WiFi.scanComplete();
            if (n < 0)
                return false;
            if(n > 0) {
                int sort[n];
                sortRSSI(&sort[0], n);
                for (int i = 0; i < n; ++i) {
                    nets[i][F("ssid")] = WiFi.SSID(sort[i]);
                    nets[i][F("rssi")] = WiFi.RSSI(sort[i]);
                }
            }
            mScanActive = false;
            WiFi.scanDelete();

            return true;
        }

    private:
        void sortRSSI(int *sort, int n) {
            for (int i = 0; i < n; i++)
                sort[i] = i;
            for (int i = 0; i < n; i++)
                for (int j = i + 1; j < n; j++)
                    if (WiFi.RSSI(sort[j]) > WiFi.RSSI(sort[i]))
                        std::swap(sort[i], sort[j]);
        }

    private:
        bool mScanActive = false;
};

#endif /*ESP32 & !ETHERNET*/
#endif /*__AHOY_WIFI_ESP32_H__*/
