//------------------------------------//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if !defined(ETHERNET)
#ifndef __AHOYWIFI_H__
#define __AHOYWIFI_H__

#include "../utils/dbg.h"
#include <Arduino.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include "ESPAsyncWebServer.h"

#include "../config/settings.h"

class app;

class ahoywifi {
    public:
        typedef std::function<void(bool)> appWifiCb;

        ahoywifi();


        void setup(settings_t *config, uint32_t *utcTimestamp, appWifiCb cb);
        void tickWifiLoop(void);
        bool getNtpTime(void);
        void scanAvailNetworks(void);
        bool getAvailNetworks(JsonObject obj);
        void setStopApAllowedMode(bool allowed) {
            mStopApAllowed = allowed;
        }
        String getStationIp(void) {
            return WiFi.localIP().toString();
        }
        void setupStation(void);

        bool getWasInCh12to14() const {
            return mWasInCh12to14;
        }

    private:
        typedef enum WiFiStatus {
            DISCONNECTED = 0,
            SCAN_READY,
            CONNECTING,
            CONNECTED,
            IN_AP_MODE,
            GOT_IP,
            IN_STA_MODE,
            RESET
        } WiFiStatus_t;

        void setupWifi(bool startAP);
        void setupAp(void);
        void sendNTPpacket(IPAddress& address);
        void sortRSSI(int *sort, int n);
        bool getBSSIDs(void);
        void connectionEvent(WiFiStatus_t status);
        bool isTimeout(uint8_t timeout) {  return (mCnt % timeout) == 0; }

#if defined(ESP8266)
        void onConnect(const WiFiEventStationModeConnected& event);
        void onGotIP(const WiFiEventStationModeGotIP& event);
        void onDisconnect(const WiFiEventStationModeDisconnected& event);
        #else
        void onWiFiEvent(WiFiEvent_t event);
        #endif
        void welcome(String ip, String mode);


        settings_t *mConfig = NULL;
        appWifiCb mAppWifiCb;

        DNSServer mDns;
        IPAddress mApIp;
        WiFiUDP mUdp; // for time server
        #if defined(ESP8266)
        WiFiEventHandler wifiConnectHandler, wifiDisconnectHandler, wifiGotIPHandler;
        #endif

        WiFiStatus_t mStaConn;
        uint8_t mCnt;
        uint32_t *mUtcTimestamp;

        uint8_t mScanCnt;
        bool mScanActive;
        bool mGotDisconnect;
        std::list<uint8_t> mBSSIDList;
        bool mStopApAllowed;
        bool mWasInCh12to14 = false;
};

#endif /*__AHOYWIFI_H__*/
#endif /* !defined(ETHERNET) */
