//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_WIFI_ESP8266_H__
#define __AHOY_WIFI_ESP8266_H__

#if defined(ESP8266)
#include <functional>
#include <list>
#include <WiFiUdp.h>
#include "AhoyNetwork.h"
#include "ESPAsyncWebServer.h"

class AhoyWifi : public AhoyNetwork {
    public:
        void begin() override {
            mAp.enable();

            WiFi.setHostname(mConfig->sys.deviceName);
            mBSSIDList.clear();
        }

        void tickNetworkLoop() override {
            if(mAp.isEnabled())
                mAp.tickLoop();

            mCnt++;

            switch(mStatus) {
                case NetworkState::DISCONNECTED:
                    if(mConnected) {
                        mConnected = false;
                        mWifiConnecting = false;
                        mOnNetworkCB(false);
                        mAp.enable();
                        MDNS.end();
                    }

                    if (WiFi.softAPgetStationNum() > 0) {
                        DBGPRINTLN(F("AP client connected"));
                    }
                    #if !defined(AP_ONLY)
                    else if (!mScanActive) {
                        DBGPRINT(F("scanning APs with SSID "));
                        DBGPRINTLN(String(mConfig->sys.stationSsid));
                        mScanCnt = 0;
                        mCnt = 0;
                        mScanActive = true;
                        WiFi.scanNetworks(true, true, 0U, ([this]() {
                            if (mConfig->sys.isHidden)
                                return (uint8_t*)NULL;
                            return (uint8_t*)(mConfig->sys.stationSsid);
                        })());
                    } else if(getBSSIDs()) {
                        mStatus = NetworkState::SCAN_READY;
                        DBGPRINT(F("connect to network '")); Serial.flush();
                        DBGPRINTLN(mConfig->sys.stationSsid);
                    }
                    #endif
                    break;

                case NetworkState::SCAN_READY:
                    mStatus = NetworkState::CONNECTING;
                    DBGPRINT(F("try to connect to BSSID:"));
                    uint8_t bssid[6];
                    for (int j = 0; j < 6; j++) {
                        bssid[j] = mBSSIDList.front();
                        mBSSIDList.pop_front();
                        DBGPRINT(" "  + String(bssid[j], HEX));
                    }
                    DBGPRINTLN("");
                    setStaticIp();
                    WiFi.begin(mConfig->sys.stationSsid, mConfig->sys.stationPwd, 0, &bssid[0]);
                    mWifiConnecting = true;
                    break;

                case NetworkState::CONNECTING:
                    if (isTimeout(TIMEOUT)) {
                        WiFi.disconnect();
                        mWifiConnecting = false;
                        mStatus = mBSSIDList.empty() ? NetworkState::DISCONNECTED : NetworkState::SCAN_READY;
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
                        MDNSResponder::hMDNSService hRes = MDNS.addService(NULL, "http", "tcp", 80);
                        MDNS.addServiceTxt(hRes, "path", "/");
                        MDNS.announce();
                        mOnNetworkCB(true);
                    }

                    MDNS.update();

                    if(WiFi.channel() > 11)
                        mWasInCh12to14 = true;
                    break;
            }
        }

        String getIp(void) override {
            return WiFi.localIP().toString();
        }

        bool getWasInCh12to14() override {
            return mWasInCh12to14;
        }

    private:
        void setStaticIp() override {
            setupIp([this](IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns1, IPAddress dns2) -> bool {
                return WiFi.config(ip, gateway, mask, dns1, dns2);
            });
        }

        bool getBSSIDs() {
            bool result = false;
            int n = WiFi.scanComplete();
            if (n < 0) {
                if (++mScanCnt < 20)
                    return false;
            }
            if(n > 0) {
                mBSSIDList.clear();
                int sort[n];
                sortRSSI(&sort[0], n);
                for (int i = 0; i < n; i++) {
                    DBGPRINT("BSSID " + String(i) + ":");
                    uint8_t *bssid = WiFi.BSSID(sort[i]);
                    for (int j = 0; j < 6; j++){
                        DBGPRINT(" " + String(bssid[j], HEX));
                        mBSSIDList.push_back(bssid[j]);
                    }
                    DBGPRINTLN("");
                }
                result = true;
            }
            mScanActive = false;
            WiFi.scanDelete();
            return result;
        }

        bool isTimeout(uint8_t timeout) {
            return ((mCnt % timeout) == 0);
        }

    private:
        uint8_t mCnt = 0;
        uint8_t mScanCnt = 0;
        std::list<uint8_t> mBSSIDList;
        bool mWasInCh12to14 = false;
        static constexpr uint8_t TIMEOUT = 20;
        static constexpr uint8_t SCAN_TIMEOUT = 10;
};

#endif /*ESP8266*/
#endif /*__AHOY_WIFI_ESP8266_H__*/
