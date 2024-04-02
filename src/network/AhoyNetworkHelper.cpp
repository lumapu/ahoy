//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#include "AhoyNetworkHelper.h"

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
