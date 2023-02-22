//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __CONFIG_OVERRIDE_H__
#define __CONFIG_OVERRIDE_H__

// override fallback WiFi info
#define FB_WIFI_OVERRIDDEN

// each ovveride must be preceeded with an #undef statement
#undef FB_WIFI_SSID
#define FB_WIFI_SSID    "MY_SSID"

// each ovveride must be preceeded with an #undef statement
#undef FB_WIFI_PWD
#define FB_WIFI_PWD     "MY_WIFI_KEY"

// ESP32 default pinout
#undef DEF_RF24_CS_PIN
#define DEF_RF24_CS_PIN         5
#undef DEF_RF24_CE_PIN
#define DEF_RF24_CE_PIN         4
#undef DEF_RF24_IRQ_PIN
#define DEF_RF24_IRQ_PIN        16


// Offset for midnight Ticker Example: 1 second before midnight (local time)
#undef MIDNIGHTTICKER_OFFSET
#define MIDNIGHTTICKER_OFFSET (mCalculatedTimezoneOffset + 1)

// To enable the json endpoint at /json
// #define ENABLE_JSON_EP

// To enable the endpoint for prometheus to scrape data from at /metrics
// #define ENABLE_PROMETHEUS_EP


#endif /*__CONFIG_OVERRIDE_H__*/
