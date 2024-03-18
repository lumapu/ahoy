//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_NETWORK_H__
#define __AHOY_NETWORK_H__

#include "AhoyNetworkHelper.h"
#include <WiFiUdp.h>
#include "../config/settings.h"
#include "../utils/helper.h"

#if defined(ESP32)
#include <ESPmDNS.h>
#else
#include <ESP8266mDNS.h>
#endif

#define NTP_PACKET_SIZE 48

class AhoyNetwork {
    public:
        typedef std::function<void(bool)> OnNetworkCB;
        typedef std::function<void(bool)> OnTimeCB;

    public:
        void setup(settings_t *config, uint32_t *utcTimestamp, OnNetworkCB onNetworkCB, OnTimeCB onTimeCB) {
            mConfig = config;
            mUtcTimestamp = utcTimestamp;
            mOnNetworkCB = onNetworkCB;
            mOnTimeCB = onTimeCB;

            #if defined(ESP32)
            WiFi.onEvent([this](WiFiEvent_t event) -> void {
                this->OnEvent(event);
            });
            #else
            wifiConnectHandler = WiFi.onStationModeConnected(
                [this](const WiFiEventStationModeConnected& event) -> void {
                OnEvent(SYSTEM_EVENT_STA_CONNECTED);
            });
            wifiGotIPHandler = WiFi.onStationModeGotIP(
                [this](const WiFiEventStationModeGotIP& event) -> void {
                OnEvent(SYSTEM_EVENT_STA_GOT_IP);
            });
            wifiDisconnectHandler = WiFi.onStationModeDisconnected(
                [this](const WiFiEventStationModeDisconnected& event) -> void {
                OnEvent(SYSTEM_EVENT_STA_DISCONNECTED);
            });
            #endif
        }

        bool isConnected() const {
            return (mStatus == NetworkState.CONNECTED);
        }

        bool updateNtpTime(void) {
            if(CONNECTED != mStatus)
                return;

            if (!mUdp.connected()) {
                IPAddress timeServer;
                if (!WiFi.hostByName(mConfig->ntp.addr, timeServer))
                    return false;
                if (!mUdp.connect(timeServer, mConfig->ntp.port))
                    return false;
            }

            mUdp.onPacket([this](AsyncUDPPacket packet) {
                this->handleNTPPacket(packet);
            });
            sendNTPpacket(timeServer);

            return true;
        }

    public:
        virtual void begin() = 0;
        virtual void tickNetworkLoop() = 0;
        virtual void connectionEvent(WiFiStatus_t status) = 0;

    protected:
        void setupIp(void) {
            if(mConfig->sys.ip.ip[0] != 0) {
                IPAddress ip(mConfig->sys.ip.ip);
                IPAddress mask(mConfig->sys.ip.mask);
                IPAddress dns1(mConfig->sys.ip.dns1);
                IPAddress dns2(mConfig->sys.ip.dns2);
                IPAddress gateway(mConfig->sys.ip.gateway);
                if(!ETH.config(ip, gateway, mask, dns1, dns2))
                    DPRINTLN(DBG_ERROR, F("failed to set static IP!"));
            }
        }

        void OnEvent(WiFiEvent_t event) {
            switch(event) {
                case SYSTEM_EVENT_STA_CONNECTED:
                    [[fallthrough]];
                case ARDUINO_EVENT_ETH_CONNECTED:
                    if(NetworkState::CONNECTED != mStatus) {
                        mStatus = NetworkState::CONNECTED;
                        DPRINTLN(DBG_INFO, F("Network connected"));
                    }
                    break;

                case SYSTEM_EVENT_STA_GOT_IP:
                    [[fallthrough]];
                case ARDUINO_EVENT_ETH_GOT_IP:
                    mStatus = NetworkState::GOT_IP;
                    break;

                case ARDUINO_EVENT_WIFI_STA_LOST_IP:
                    [[fallthrough]];
                case ARDUINO_EVENT_WIFI_STA_STOP:
                    [[fallthrough]];
                case SYSTEM_EVENT_STA_DISCONNECTED:
                    [[fallthrough]];
                case ARDUINO_EVENT_ETH_STOP:
                    [[fallthrough]];
                case ARDUINO_EVENT_ETH_DISCONNECTED:
                    mStatus = NetworkState::DISCONNECTED;
                    break;

                default:
                    break;
            }
        }

    private:
        void sendNTPpacket(IPAddress& address) {
            //DPRINTLN(DBG_VERBOSE, F("wifi::sendNTPpacket"));
            uint8_t buf[NTP_PACKET_SIZE];
            memset(buf, 0, NTP_PACKET_SIZE);

            buf[0] = 0b11100011; // LI, Version, Mode
            buf[1] = 0;          // Stratum
            buf[2] = 6;          // Max Interval between messages in seconds
            buf[3] = 0xEC;       // Clock Precision
            // bytes 4 - 11 are for Root Delay and Dispersion and were set to 0 by memset
            buf[12] = 49;        // four-byte reference ID identifying
            buf[13] = 0x4E;
            buf[14] = 49;
            buf[15] = 52;

            //mUdp.beginPacket(address, 123); // NTP request, port 123
            mUdp.write(buf, NTP_PACKET_SIZE);
            //mUdp.endPacket();
        }

        void handleNTPPacket(AsyncUDPPacket packet) {
            char buf[80];

            memcpy(buf, packet.data(), sizeof(buf));

            unsigned long highWord = word(buf[40], buf[41]);
            unsigned long lowWord = word(buf[42], buf[43]);

            // combine the four bytes (two words) into a long integer
            // this is NTP time (seconds since Jan 1 1900):
            unsigned long secsSince1900 = highWord << 16 | lowWord;

            *mUtcTimestamp = secsSince1900 - 2208988800UL; // UTC time
            DPRINTLN(DBG_INFO, "[NTP]: " + ah::getDateTimeStr(*mUtcTimestamp) + " UTC");
            mOnTimeCB(true);
            mUdp.close();
        }

    protected:
        enum class NetworkState : uint8_t {
            DISCONNECTED,
            CONNECTING,
            CONNECTED,
            IN_AP_MODE,
            GOT_IP,
            IN_STA_MODE,
            RESET,
            SCAN_READY
        };

    protected:
        settings_t *mConfig = nullptr;
        uint32_t *mUtcTimestamp = nullptr;

        OnNetworkCB mOnNetworkCB;
        OnTimeCB mOnTimeCB;

        NetworkState mStatus = NetworkState.DISCONNECTED;

        WiFiUDP mUdp; // for time server
        DNSServer mDns;
};

#endif /*__AHOY_NETWORK_H__*/
