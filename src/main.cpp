//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "utils/dbg.h"
#include "app.h"
#include "config/config.h"

app myApp;

//-----------------------------------------------------------------------------
void setup() {
    myApp.setup();
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}
