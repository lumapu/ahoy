//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __CONFIG_H__
#define __CONFIG_H__


//-------------------------------------
// WIFI CONFIGURATION
//-------------------------------------

// Fallback WiFi Info
#define FB_WIFI_SSID    "YOUR_WIFI_SSID"
#define FB_WIFI_PWD     "YOUR_WIFI_PWD"


// Access Point Info
// In case there is no WiFi Network or Ahoy can not connect to it, it will act as an Access Point

#define WIFI_AP_SSID    "AHOY-DTU"
#define WIFI_AP_PWD     "esp_8266"

// If the next line is uncommented, Ahoy will stay in access point mode all the time
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

// default pinout (GPIO Number)
#define DEF_RF24_CS_PIN         15
#define DEF_RF24_CE_PIN         2
#define DEF_RF24_IRQ_PIN        0

// default radio ID (the ending 01ULL NEEDS to remain unmodified!)
#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)

// default NRF24 power, possible values (0 - 3)
#define DEF_AMPLIFIERPOWER      1

// number of packets hold in buffer
#define PACKET_BUFFER_SIZE      30

// number of configurable inverters
#define MAX_NUM_INVERTERS       4

// default serial interval
#define SERIAL_INTERVAL         5

// default send interval
#define SEND_INTERVAL           30

// maximum human readable inverter name length
#define MAX_NAME_LENGTH         16

// maximum buffer length of packet received / sent to RF24 module
#define MAX_RF_PAYLOAD_SIZE     32

// maximum total payload buffers (must be greater than the number of received frame fragments)
#define MAX_PAYLOAD_ENTRIES     10

// maximum requests for retransmits per payload (per inverter)
#define DEF_MAX_RETRANS_PER_PYLD 5

// number of seconds since last successful response, before inverter is marked inactive
#define INACT_THRES_SEC         300

// threshold of minimum power on which the inverter is marked as inactive
#define INACT_PWR_THRESH        3

// default NTP server uri
#define DEF_NTP_SERVER_NAME     "pool.ntp.org"

// default NTP server port
#define DEF_NTP_PORT            123

// NTP refresh interval in ms (default 12h)
#define NTP_REFRESH_INTERVAL    12 * 3600 * 1000

// default mqtt interval
#define MQTT_INTERVAL           60

// default MQTT broker uri
#define DEF_MQTT_BROKER         "\0"

// default MQTT port
#define DEF_MQTT_PORT           1883

// default MQTT user
#define DEF_MQTT_USER           "\0"

// default MQTT pwd
#define DEF_MQTT_PWD           "\0"

// default MQTT topic
#define DEF_MQTT_TOPIC         "inverter"

//default MQTT Message Inverter Status
#define DEF_MQTT_IV_MESSAGE_NOT_AVAIL_AND_NOT_PRODUCED          "not available and not producing"   // STATUS 0
#define DEF_MQTT_IV_MESSAGE_INVERTER_AVAIL_AND_NOT_PRODUCED     "available and not producing"       // STATUS 1
#define DEF_MQTT_IV_MESSAGE_INVERTER_AVAIL_AND_PRODUCED         "available and producing"           // STATUS 2

#if __has_include("config_override.h")
    #include "config_override.h"
#endif

#endif /*__CONFIG_H__*/
