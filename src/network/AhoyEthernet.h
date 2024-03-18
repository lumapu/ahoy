//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_ETHERNET_H__
#define __AHOY_ETHERNET_H__

#include <functional>
#include <AsyncUDP.h>
#include <ETH.h>
#include "AhoyEthernetSpi.h"
#include "AhoyEthernet.h"

class AhoyEthernet : public AhoyNetwork {
    public:
        void begin() override {
            setupIp([this](IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns1, IPAddress dns2) -> bool {
                return ETH.config(ip, gateway, mask, dns1, dns2);
            });
        }

        void tickNetworkLoop() override {
            switch(mState) {
                case NetworkState::DISCONNECTED:
                    break;
            }
        }

    private:

        /*switch (event) {
            case ARDUINO_EVENT_ETH_START:
                DPRINTLN(DBG_VERBOSE, F("ETH Started"));

                if(String(mConfig->sys.deviceName) != "")
                    ETH.setHostname(mConfig->sys.deviceName);
                else
                    ETH.setHostname(F("ESP32_W5500"));
                break;

            case ARDUINO_EVENT_ETH_CONNECTED:
                DPRINTLN(DBG_VERBOSE, F("ETH Connected"));
                break;

            case ARDUINO_EVENT_ETH_GOT_IP:
                if (!mEthConnected) {
                    welcome(ETH.localIP().toString(), F(" (Station)"));

                    mEthConnected = true;
                    mOnNetworkCB(true);
                }

                if (!MDNS.begin(mConfig->sys.deviceName)) {
                    DPRINTLN(DBG_ERROR, F("Error setting up MDNS responder!"));
                } else {
                    DBGPRINT(F("mDNS established: "));
                    DBGPRINT(mConfig->sys.deviceName);
                    DBGPRINTLN(F(".local"));
                }
                break;

            case ARDUINO_EVENT_ETH_DISCONNECTED:
                DPRINTLN(DBG_INFO, F("ETH Disconnected"));
                mEthConnected = false;
                mUdp.close();
                mOnNetworkCB(false);
                break;

            case ARDUINO_EVENT_ETH_STOP:
                DPRINTLN(DBG_INFO, F("ETH Stopped"));
                mEthConnected = false;
                mUdp.close();
                mOnNetworkCB(false);
                break;

            default:
                break;
        }*/
};

#endif /*__AHOY_ETHERNET_H__*/
