//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
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
IRAM_ATTR void handleHmsIntr(void) {
    myApp.handleHmsIntr();
}


//-----------------------------------------------------------------------------
void setup() {
    myApp.setup();

    attachInterrupt(digitalPinToInterrupt(myApp.getIrqPin()), handleIntr, FALLING);
    attachInterrupt(digitalPinToInterrupt(myApp.getHmsIrqPin()), handleHmsIntr, RISING);
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}
