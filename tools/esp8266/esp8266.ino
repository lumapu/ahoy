#include "app.h"

app myApp;

//-----------------------------------------------------------------------------
void setup() {
    pinMode(RF24_IRQ_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(RF24_IRQ_PIN), handleIntr, FALLING);

    // AP name, password, timeout
    myApp.setup("ESP AHOY", "esp_8266", 15);
}


//-----------------------------------------------------------------------------
void loop() {
    myApp.loop();
}


//-----------------------------------------------------------------------------
ICACHE_RAM_ATTR void handleIntr(void) {
    myApp.handleIntr();
}

