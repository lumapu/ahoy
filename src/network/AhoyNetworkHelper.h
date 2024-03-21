//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_NETWORK_HELPER_H__
#define __AHOY_NETWORK_HELPER_H__

#include "../utils/dbg.h"
#include <Arduino.h>
#if defined(ESP32)
    #include <WiFi.h>
    #include <WiFiType.h>
    #include <ESPmDNS.h>
#else
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
    //#include <WiFiUdp.h>
    #include "ESPAsyncUDP.h"

    enum {
        SYSTEM_EVENT_STA_CONNECTED = 1,
        ARDUINO_EVENT_ETH_CONNECTED,
        SYSTEM_EVENT_STA_GOT_IP,
        ARDUINO_EVENT_ETH_GOT_IP,
        ARDUINO_EVENT_WIFI_STA_LOST_IP,
        ARDUINO_EVENT_WIFI_STA_STOP,
        SYSTEM_EVENT_STA_DISCONNECTED,
        ARDUINO_EVENT_ETH_STOP,
        ARDUINO_EVENT_ETH_DISCONNECTED
    };
#endif
#include <DNSServer.h>

namespace ah {
    void welcome(String ip, String info);
}

#endif /*__AHOY_NETWORK_HELPER_H__*/
