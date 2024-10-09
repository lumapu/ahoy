//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_ETHERNET_H__
#define __AHOY_ETHERNET_H__

#if defined(ETHERNET)
#include <functional>
#include <AsyncUDP.h>
#include <ETH.h>
#include "AhoyEthernetSpi.h"
#include "AhoyNetwork.h"
#include "AhoyWifiEsp32.h"

class AhoyEthernet : public AhoyWifi {
    private:
        enum class Mode {
            WIRED,
            WIRELESS
        };

    public:
        AhoyEthernet()
            : mMode (Mode::WIRELESS) {}

        virtual void begin() override {
            mMode = Mode::WIRELESS;
            mAp.enable();
            AhoyWifi::begin();

            if(!mConfig->sys.eth.enabled)
                return;

            mEthSpi.begin(mConfig->sys.eth.pinMiso, mConfig->sys.eth.pinMosi, mConfig->sys.eth.pinSclk, mConfig->sys.eth.pinCs, mConfig->sys.eth.pinIrq, mConfig->sys.eth.pinRst);
            ETH.setHostname(mConfig->sys.deviceName);
        }

        virtual String getIp(void) override {
            if(Mode::WIRELESS == mMode)
                return AhoyWifi::getIp();
            else
                return ETH.localIP().toString();
        }

        virtual String getMac(void) override {
            if(Mode::WIRELESS == mMode)
                return AhoyWifi::getMac();
            else
                return mEthSpi.macAddress();
        }

        virtual bool isWiredConnection() override {
            return (Mode::WIRED == mMode);
        }

    private:
        virtual void OnEvent(WiFiEvent_t event) override {
            switch(event) {
                case ARDUINO_EVENT_ETH_CONNECTED:
                    mMode = Mode::WIRED; // needed for static IP
                    [[fallthrough]];
                case SYSTEM_EVENT_STA_CONNECTED:
                    mWifiConnecting = false;
                    if(NetworkState::CONNECTED != mStatus) {
                        if(ARDUINO_EVENT_ETH_CONNECTED == event)
                            WiFi.disconnect();

                        mStatus = NetworkState::CONNECTED;
                        DPRINTLN(DBG_INFO, F("Network connected"));
                        setStaticIp();
                    }
                    break;

                case SYSTEM_EVENT_STA_GOT_IP:
                    mStatus = NetworkState::GOT_IP;
                    if(mAp.isEnabled())
                        mAp.disable();

                    mMode = Mode::WIRELESS;
                    if(!mConnected) {
                        mConnected = true;
                        ah::welcome(WiFi.localIP().toString(), F("Station WiFi"));
                        MDNS.begin(mConfig->sys.deviceName);
                        mOnNetworkCB(true);
                    }
                    break;

                case ARDUINO_EVENT_ETH_GOT_IP:
                    mStatus = NetworkState::GOT_IP;
                    mMode = Mode::WIRED;
                    if(!mConnected) {
                        mAp.disable();
                        mConnected = true;
                        ah::welcome(ETH.localIP().toString(), F("Station Ethernet"));
                        MDNS.begin(mConfig->sys.deviceName);
                        mOnNetworkCB(true);
                        WiFi.disconnect();
                    }
                    break;

                case ARDUINO_EVENT_ETH_STOP:
                    [[fallthrough]];
                case ARDUINO_EVENT_ETH_DISCONNECTED:
                    mStatus = NetworkState::DISCONNECTED;
                    if(mConnected) {
                        mMode = Mode::WIRELESS;
                        mConnected = false;
                        mOnNetworkCB(false);
                        MDNS.end();
                        AhoyWifi::begin();
                    }
                    break;

                case ARDUINO_EVENT_WIFI_STA_LOST_IP:
                    [[fallthrough]];
                case ARDUINO_EVENT_WIFI_STA_STOP:
                    [[fallthrough]];
                case SYSTEM_EVENT_STA_DISCONNECTED:
                    mStatus = NetworkState::DISCONNECTED;
                    if(mConnected && (Mode::WIRELESS == mMode)) {
                        mConnected = false;
                        mOnNetworkCB(false);
                        MDNS.end();
                        AhoyWifi::begin();
                    }
                    break;

                default:
                    break;
            }
        }

        void setStaticIp() override {
            setupIp([this](IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns1, IPAddress dns2) -> bool {
                if(Mode::WIRELESS == mMode)
                    return WiFi.config(ip, gateway, mask, dns1, dns2);
                else
                    return ETH.config(ip, gateway, mask, dns1, dns2);
            });
        }

    private:
        AhoyEthernetSpi mEthSpi;
        Mode mMode;

};

#endif /*ETHERNET*/
#endif /*__AHOY_ETHERNET_H__*/
