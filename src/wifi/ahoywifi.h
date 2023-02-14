//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

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
        void getAvailNetworks(JsonObject obj);

    private:
        typedef enum WiFiStatus {
            DISCONNECTED = 0,
            CONNECTING,
            CONNECTED,
            IN_AP_MODE,
            GOT_IP
        } WiFiStatus_t;

        void setupWifi(bool startAP);
        void setupAp(void);
        void setupStation(void);
        void sendNTPpacket(IPAddress& address);
        void sortRSSI(int *sort, int n);
        void getBSSIDs(void);
        void connectionEvent(WiFiStatus_t status);
        #if defined(ESP8266)
        void onConnect(const WiFiEventStationModeConnected& event);
        void onGotIP(const WiFiEventStationModeGotIP& event);
        void onDisconnect(const WiFiEventStationModeDisconnected& event);
        #else
        void onWiFiEvent(WiFiEvent_t event);
        #endif
        void welcome(String ip, String mode);


        settings_t *mConfig;
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
        std::list<uint8_t> mBSSIDList;
};

#endif /*__AHOYWIFI_H__*/
