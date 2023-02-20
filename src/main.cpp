//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "utils/dbg.h"
#include "app.h"

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

    if(myApp.getNrfEnabled())
        attachInterrupt(digitalPinToInterrupt(myApp.getNrfIrqPin()), handleIntr, FALLING);
    if(myApp.getCmtEnabled())
        attachInterrupt(digitalPinToInterrupt(myApp.getCmtIrqPin()), handleHmsIntr, RISING);
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}
