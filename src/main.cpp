//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#include "utils/dbg.h"
#include "app.h"
#include "config/config.h"


app myApp;

//-----------------------------------------------------------------------------
IRAM_ATTR void handleIntr(void) {
    myApp.handleIntr();
}


//-----------------------------------------------------------------------------
void setup() {
    myApp.setup();

    // TODO: move to HmRadio
    attachInterrupt(digitalPinToInterrupt(myApp.getIrqPin()), handleIntr, FALLING);
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}
