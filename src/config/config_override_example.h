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

// ESP32-S3 example pinout
#undef DEF_CS_PIN
#define DEF_CS_PIN 37
#undef DEF_CE_PIN
#define DEF_CE_PIN 38
#undef DEF_IRQ_PIN
#define DEF_IRQ_PIN 47
#undef DEF_MISO_PIN
#define DEF_MISO_PIN 48
#undef DEF_MOSI_PIN
#define DEF_MOSI_PIN 35
#undef DEF_SCLK_PIN
#define DEF_SCLK_PIN 36

// Offset for midnight Ticker Example: 1 second before midnight (local time)
#undef MIDNIGHTTICKER_OFFSET
#define MIDNIGHTTICKER_OFFSET (mCalculatedTimezoneOffset + 1)

// To enable the endpoint for prometheus to scrape data from at /metrics
// #define ENABLE_PROMETHEUS_EP


#endif /*__CONFIG_OVERRIDE_H__*/
