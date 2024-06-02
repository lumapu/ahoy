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

class AhoyEthernet : public AhoyNetwork {
    public:
        void begin() override {
            mAp.enable();

            if(!mConfig->sys.eth.enabled)
                return;

            mEthSpi.begin(mConfig->sys.eth.pinMiso, mConfig->sys.eth.pinMosi, mConfig->sys.eth.pinSclk, mConfig->sys.eth.pinCs, mConfig->sys.eth.pinIrq, mConfig->sys.eth.pinRst);
            ETH.setHostname(mConfig->sys.deviceName);
        }

        void OnEvent(WiFiEvent_t event) override {
            switch(event) {
                case ARDUINO_EVENT_ETH_CONNECTED:
                    if(NetworkState::CONNECTED != mStatus) {
                        mStatus = NetworkState::CONNECTED;
                        DPRINTLN(DBG_INFO, F("Network connected"));
                        setStaticIp();
                    }
                    break;

                case ARDUINO_EVENT_ETH_GOT_IP:
                    mStatus = NetworkState::GOT_IP;
                    if(!mConnected) {
                        mAp.disable();
                        mConnected = true;
                        ah::welcome(ETH.localIP().toString(), F("Station"));
                        MDNS.begin(mConfig->sys.deviceName);
                        mOnNetworkCB(true);
                    }
                    break;

                case ARDUINO_EVENT_ETH_STOP:
                    [[fallthrough]];
                case ARDUINO_EVENT_ETH_DISCONNECTED:
                    mStatus = NetworkState::DISCONNECTED;
                    if(mConnected) {
                        mConnected = false;
                        mOnNetworkCB(false);
                        mAp.enable();
                    }
                    break;

                default:
                    break;
            }
        }

        void tickNetworkLoop() override {
            if(mAp.isEnabled())
                mAp.tickLoop();
        }

        String getIp(void) override {
            return ETH.localIP().toString();
        }

    private:
        void setStaticIp() override {
            setupIp([this](IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns1, IPAddress dns2) -> bool {
                return ETH.config(ip, gateway, mask, dns1, dns2);
            });
        }

    private:
        AhoyEthernetSpi mEthSpi;
};

#endif /*ETHERNET*/
#endif /*__AHOY_ETHERNET_H__*/
