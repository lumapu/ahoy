//-----------------------------------------------------------------------------
// 2024 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __CONFIG_H__
#define __CONFIG_H__


// globally used
#define DEF_PIN_OFF         255


//-------------------------------------
// WIFI CONFIGURATION
//-------------------------------------

// Fallback WiFi Info
#define FB_WIFI_SSID    ""
#define FB_WIFI_PWD     ""

// Access Point Info
// In case there is no WiFi Network or Ahoy can not connect to it, it will act as an Access Point

#define WIFI_AP_SSID    "AHOY-DTU"
#define WIFI_AP_PWD     "esp_8266"

// If the next line is uncommented, Ahoy will stay in access point mode all the time
//#define AP_ONLY
#if defined(AP_ONLY)
    #if defined(ENABLE_MQTT)
        #undef ENABLE_MQTT
    #endif
#endif

// timeout for automatic logoff (20 minutes)
#define LOGOUT_TIMEOUT      (20 * 60)


//-------------------------------------
// MODULE SELECTOR - done by platform.ini
//-------------------------------------

// MqTT connection
//#define ENABLE_MQTT

// display plugin
//#define PLUGIN_DISPLAY

// history graph (WebUI)
//#define ENABLE_HISTORY

// inverter simulation
//#define ENABLE_SIMULATOR

// to enable the syslog logging (will disable web-serial)
//#define ENABLE_SYSLOG



//-------------------------------------
// CONFIGURATION - COMPILE TIME
//-------------------------------------

// ethernet
#if defined(ETHERNET)
    #define ETH_SPI_HOST            SPI2_HOST
    #define ETH_SPI_CLOCK_MHZ       25
    #ifndef DEF_ETH_IRQ_PIN
        #define DEF_ETH_IRQ_PIN     4
    #endif
    #ifndef DEF_ETH_MISO_PIN
        #define DEF_ETH_MISO_PIN    12
    #endif
    #ifndef DEF_ETH_MOSI_PIN
        #define DEF_ETH_MOSI_PIN    13
    #endif
    #ifndef DEF_ETH_SCK_PIN
        #define DEF_ETH_SCK_PIN     14
    #endif
    #ifndef DEF_ETH_CS_PIN
        #define DEF_ETH_CS_PIN      15
    #endif
    #ifndef DEF_ETH_RST_PIN
        #define DEF_ETH_RST_PIN     DEF_PIN_OFF
    #endif
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
    #ifndef DEF_NRF_CS_PIN
        #define DEF_NRF_CS_PIN          5
    #endif
    #ifndef DEF_NRF_CE_PIN
        #define DEF_NRF_CE_PIN          4
    #endif
    #ifndef DEF_NRF_IRQ_PIN
        #define DEF_NRF_IRQ_PIN         16
    #endif
    #ifndef DEF_NRF_MISO_PIN
        #define DEF_NRF_MISO_PIN        19
    #endif
    #ifndef DEF_NRF_MOSI_PIN
        #define DEF_NRF_MOSI_PIN        23
    #endif
    #ifndef DEF_NRF_SCLK_PIN
        #define DEF_NRF_SCLK_PIN        18
    #endif

#if defined(ETHERNET) && !defined(SPI_HAL)
    #ifndef DEF_CMT_SPI_HOST
        #define DEF_CMT_SPI_HOST        SPI3_HOST
    #endif
#else
    #ifndef DEF_CMT_SPI_HOST
        #define DEF_CMT_SPI_HOST        SPI2_HOST
    #endif
#endif /* defined(ETHERNET) */

    #ifndef DEF_CMT_SCLK
        #define DEF_CMT_SCLK            12
    #endif
    #ifndef DEF_CMT_SDIO
        #define DEF_CMT_SDIO            14
    #endif
    #ifndef DEF_CMT_CSB
        #define DEF_CMT_CSB             27
    #endif
    #ifndef DEF_CMT_FCSB
        #define DEF_CMT_FCSB            26
    #endif
    #ifndef DEF_CMT_IRQ
        #define DEF_CMT_IRQ             34
    #endif
    #ifndef DEF_MOTION_SENSOR_PIN
        #define DEF_MOTION_SENSOR_PIN   DEF_PIN_OFF
    #endif
#else // ESP8266
    #ifndef DEF_NRF_CS_PIN
        #define DEF_NRF_CS_PIN          15
    #endif
    #ifndef DEF_NRF_CE_PIN
        #define DEF_NRF_CE_PIN          0
    #endif
    #ifndef DEF_NRF_IRQ_PIN
        #define DEF_NRF_IRQ_PIN         2
    #endif
    // these are given to relay the correct values via API
    // they cannot actually be moved for ESP82xx models
    #ifndef DEF_NRF_MISO_PIN
        #define DEF_NRF_MISO_PIN        12
    #endif
    #ifndef DEF_NRF_MOSI_PIN
        #define DEF_NRF_MOSI_PIN        13
    #endif
    #ifndef DEF_NRF_SCLK_PIN
        #define DEF_NRF_SCLK_PIN        14
    #endif
    #ifndef DEF_MOTION_SENSOR_PIN
        #define DEF_MOTION_SENSOR_PIN   A0
    #endif
#endif
#ifndef DEF_LED0
    #define DEF_LED0                DEF_PIN_OFF
#endif
#ifndef DEF_LED1
    #define DEF_LED1                DEF_PIN_OFF
#endif
#ifndef DEF_LED2
    #define DEF_LED2                DEF_PIN_OFF
#endif
#ifdef LED_ACTIVE_HIGH
    #define LED_HIGH_ACTIVE         true
#else
    #define LED_HIGH_ACTIVE         false
#endif

// number of packets hold in buffer
#define PACKET_BUFFER_SIZE      30

// number of configurable inverters
#if defined(ESP32)
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
        #define MAX_NUM_INVERTERS   32
    #else
        #define MAX_NUM_INVERTERS   16
    #endif
#else
    #define MAX_NUM_INVERTERS   4
#endif

// default send interval
#define SEND_INTERVAL           15

// maximum human readable inverter name length
#define MAX_NAME_LENGTH         16

// maximum buffer length of packet received / sent to RF24 module
#define MAX_RF_PAYLOAD_SIZE     32

// maximum total payload buffers (must be greater than the number of received frame fragments)
#define MAX_PAYLOAD_ENTRIES     20

// number of seconds since last successful response, before inverter is marked inactive
#define INVERTER_INACT_THRES_SEC    5*60

// number of seconds since last successful response, before inverter is marked offline
#define INVERTER_OFF_THRES_SEC      15*60

// threshold of minimum power on which the inverter is marked as inactive
#define INACT_PWR_THRESH        0

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

// maximum custom link length
#define MAX_CUSTOM_LINK_LEN         100
#define MAX_CUSTOM_LINK_TEXT_LEN    32
// syslog settings
#ifdef ENABLE_SYSLOG
#define SYSLOG_HOST "<hostname-or-ip-address-of-syslog-server>"
#define SYSLOG_APP  "ahoy"
#define SYSLOG_FACILITY FAC_USER
#define SYSLOG_PORT 514
#endif

#if __has_include("config_override.h")
    #include "config_override.h"
#endif

#endif /*__CONFIG_H__*/
