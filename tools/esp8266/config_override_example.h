//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __CONFIG_OVERRIDE_H__
#define __CONFIG_OVERRIDE_H__

// override fallback WiFi info

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

#endif /*__CONFIG_OVERRIDE_H__*/
