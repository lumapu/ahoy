//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
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

    if(myApp.getNrfEnabled()) {
        if(DEF_PIN_OFF != myApp.getNrfIrqPin())
            attachInterrupt(digitalPinToInterrupt(myApp.getNrfIrqPin()), handleIntr, FALLING);
    }
    if(myApp.getCmtEnabled()) {
        if(DEF_PIN_OFF != myApp.getCmtIrqPin())
            attachInterrupt(digitalPinToInterrupt(myApp.getCmtIrqPin()), handleHmsIntr, RISING);
    }
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}
