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

// timeout for automatic logoff (20 minutes)
#define LOGOUT_TIMEOUT      (20 * 60)

//-------------------------------------
// CONFIGURATION - COMPILE TIME
//-------------------------------------

// ethernet

#if defined(ETHERNET)
    #define ETH_SPI_HOST            SPI2_HOST
    #define ETH_SPI_CLOCK_MHZ       25
    #define ETH_INT_GPIO            4
    #define ETH_MISO_GPIO           12
    #define ETH_MOSI_GPIO           13
    #define ETH_SCK_GPIO            14
    #define ETH_CS_PIN              15
#else /* defined(ETHERNET) */
// time in seconds how long the station info (ssid + pwd) will be tried
#define WIFI_TRY_CONNECT_TIME   30

// time during the ESP will act as access point on connection failure (to
// station) in seconds
#define WIFI_AP_ACTIVE_TIME     60
#endif /* defined(ETHERNET) */

// default device name
#define DEF_DEVICE_NAME         "AHOY-DTU"

// default pinout (GPIO Number)
#if defined(ESP32)
    // this is the default ESP32 (son-S) pinout on the WROOM modules for VSPI,
    // for the ESP32-S3 there is no sane 'default', as it has full flexibility
    // to map its two HW SPIs anywhere and PCBs differ materially,
    // so it has to be selected in the Web UI
    #define DEF_CS_PIN              5
    #define DEF_CE_PIN              17
    #define DEF_IRQ_PIN             16
    #define DEF_MISO_PIN            19
    #define DEF_MOSI_PIN            23
    #define DEF_SCLK_PIN            18
#else
    #define DEF_CS_PIN              15
    #define DEF_CE_PIN              0
    #define DEF_IRQ_PIN             2
    // these are given to relay the correct values via API
    // they cannot actually be moved for ESP82xx models
    #define DEF_MISO_PIN            12
    #define DEF_MOSI_PIN            13
    #define DEF_SCLK_PIN            14
#endif

// default NRF24 power, possible values (0 - 3)
#define DEF_AMPLIFIERPOWER      1

// number of packets hold in buffer
#define PACKET_BUFFER_SIZE      30

// number of configurable inverters
#if defined(ESP32)
    #define MAX_NUM_INVERTERS   16
#else
    #define MAX_NUM_INVERTERS   4
#endif

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
#define INVERTER_INACT_THRES_SEC    5*60

// number of seconds since last successful response, before inverter is marked offline
#define INVERTER_OFF_THRES_SEC      15*60

// threshold of minimum power on which the inverter is marked as inactive
#define INACT_PWR_THRESH        3

// Timezone
#define TIMEZONE                1

// default NTP server uri
#define DEF_NTP_SERVER_NAME     "pool.ntp.org"

// default NTP server port
#define DEF_NTP_PORT            123

// NTP refresh interval in ms (default 12h)
#define NTP_REFRESH_INTERVAL    12 * 3600 * 1000

// default mqtt interval
#define MQTT_INTERVAL           90

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

// discovery prefix
#define MQTT_DISCOVERY_PREFIX   "homeassistant"

// reconnect delay
#define MQTT_RECONNECT_DELAY    5000

// Offset for midnight Ticker
// relative to UTC
//   may be negative for later in the next day or positive for earlier in previous day
//   may contain variable like mCalculatedTimezoneOffset
// must be in parentheses
#define MIDNIGHTTICKER_OFFSET (-1)

#if __has_include("config_override.h")
    #include "config_override.h"
#endif

#endif /*__CONFIG_H__*/
