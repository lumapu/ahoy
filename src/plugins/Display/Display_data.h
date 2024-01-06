#include "../../utils/helper.h"

#ifndef __DISPLAY_DATA__
#define __DISPLAY_DATA__

struct DisplayData {
    const char *version=nullptr;
    float totalPower=0.0f;      // indicate current power (W)
    float totalYieldDay=0.0f;   // indicate day yield (Wh)
    float totalYieldTotal=0.0f; // indicate total yield (kWh)
    uint32_t utcTs=0;           // indicate absolute timestamp (utc unix time). 0 = time is not synchonized
    uint8_t nrProducing=0;      // indicate number of producing inverters
    uint8_t nrSleeping=0;       // indicate number of sleeping inverters
    bool WifiSymbol = false;    // indicate if WiFi is connected
    bool RadioSymbol = false;   // indicate if radio module is connecting and working
    bool MQTTSymbol = false;    // indicate if MQTT is connected
    int8_t WifiRSSI=SCHAR_MIN;  // indicate RSSI value for WiFi
    int8_t RadioRSSI=SCHAR_MIN; // indicate RSSI value for radio
    IPAddress ipAddress;        // indicate ip adress of ahoy
};

#endif /*__DISPLAY_DATA__*/
