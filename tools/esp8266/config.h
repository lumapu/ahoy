//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __CONFIG_H__
#define __CONFIG_H__

// fallback WiFi info
#define FB_WIFI_SSID    "YOUR_WIFI_SSID"
#define FB_WIFI_PWD     "YOUR_WIFI_PWD"


// access point info
#define WIFI_AP_SSID    "AHOY-DTU"
#define WIFI_AP_PWD     "esp_8266"
// stay in access point mode all the time
//#define AP_ONLY


//-------------------------------------
// CONFIGURATION - COMPILE TIME
//-------------------------------------
// time in seconds how long the station info (ssid + pwd) will be tried
#define WIFI_TRY_CONNECT_TIME   30

// time during the ESP will act as access point on connection failure (to
// station) in seconds
#define WIFI_AP_ACTIVE_TIME     60

// default device name
#define DEF_DEVICE_NAME         "AHOY-DTU"

// number of packets hold in buffer
#define PACKET_BUFFER_SIZE      30

// number of configurable inverters
#define MAX_NUM_INVERTERS       3

// default serial interval
#define SERIAL_INTERVAL     5

// default send interval
#define SEND_INTERVAL       30

// default mqtt interval
#define MQTT_INTERVAL       60

// maximum human readable inverter name length
#define MAX_NAME_LENGTH         16

// maximum buffer length of packet received / sent to RF24 module
#define MAX_RF_PAYLOAD_SIZE     32

// maximum total payload buffers (must be greater than the number of received frame fragments)
#define MAX_PAYLOAD_ENTRIES     4

// maximum requests for retransmits per payload (per inverter)
#define DEF_MAX_RETRANS_PER_PYLD 5

// number of seconds since last successful response, before inverter is marked inactive
#define INACT_THRES_SEC         300

// threshold of minimum power on which the inverter is marked as inactive
#define INACT_PWR_THRESH        3

// changes the style of "/setup" page, visualized = nicer
#define LIVEDATA_VISUALIZED

#endif /*__CONFIG_H__*/
