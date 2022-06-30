//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "dbg.h"
#include "app.h"
#include "config.h"

app myApp;

//-----------------------------------------------------------------------------
IRAM_ATTR void handleIntr(void) {
    myApp.handleIntr();
}


//-----------------------------------------------------------------------------
void setup() {
    myApp.setup(WIFI_TRY_CONNECT_TIME);

    // TODO: move to HmRadio
    attachInterrupt(digitalPinToInterrupt(myApp.getIrqPin()), handleIntr, FALLING);
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}
