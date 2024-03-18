//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __AHOY_NETWORK_HELPER_H__
#define __AHOY_NETWORK_HELPER_H__

#include "../utils/dbg.h"
#include <Arduino.h>
#include <WiFiType.h>
#include <WiFi.h>
#include <DNSServer.h>

namespace ah {
    void welcome(String ip, String info) {
        DBGPRINTLN(F("\n\n-------------------"));
        DBGPRINTLN(F("Welcome to AHOY!"));
        DBGPRINT(F("\npoint your browser to http://"));
        DBGPRINT(ip);
        DBGPRINT(" (");
        DBGPRINT(info);
        DBGPRINTLN(")");
        DBGPRINTLN(F("to configure your device"));
        DBGPRINTLN(F("-------------------\n"));
    }
}

#endif /*__AHOY_NETWORK_HELPER_H__*/
