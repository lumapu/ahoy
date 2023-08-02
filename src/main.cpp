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
#ifdef ESP32
IRAM_ATTR void handleHmsIntr(void) {
    myApp.handleHmsIntr();
}
#endif

//-----------------------------------------------------------------------------
void setup() {
    myApp.setup();

    if(myApp.getNrfEnabled()) {
        if(DEF_PIN_OFF != myApp.getNrfIrqPin())
            attachInterrupt(digitalPinToInterrupt(myApp.getNrfIrqPin()), handleIntr, FALLING);
    }
    #ifdef ESP32
    if(myApp.getCmtEnabled()) {
        if(DEF_PIN_OFF != myApp.getCmtIrqPin())
            attachInterrupt(digitalPinToInterrupt(myApp.getCmtIrqPin()), handleHmsIntr, RISING);
    }
    #endif
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}
