
#include "Arduino.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>

#include <ESP8266HTTPUpdateServer.h>
#include "app.h"

app myApp;

//-----------------------------------------------------------------------------
void setup() {
    // AP name, password, timeout
    myApp.setup("ESP AHOY", "esp_8266", 15);

    // TODO: move to HmRadio
    attachInterrupt(digitalPinToInterrupt(myApp.getIrqPin()), handleIntr, FALLING);
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}


//-----------------------------------------------------------------------------
ICACHE_RAM_ATTR void handleIntr(void) {
    myApp.handleIntr();
}
