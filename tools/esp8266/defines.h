#ifndef __DEFINES_H__
#define __DEFINES_H__


//-------------------------------------
// PINOUT
//-------------------------------------
#define RF24_IRQ_PIN        4
#define RF24_CE_PIN         5
#define RF24_CS_PIN         15



//-------------------------------------
// CONFIGURATION - COMPILE TIME
//-------------------------------------
#define PACKET_BUFFER_SIZE      30
#define MAX_NUM_INVERTERS       3


//-------------------------------------
// VERSION
//-------------------------------------
#define VERSION_MAJOR       0
#define VERSION_MINOR       2
#define VERSION_PATCH       1


//-------------------------------------
// EEPROM
//-------------------------------------
#define SSID_LEN            32
#define PWD_LEN             32
#define DEVNAME_LEN         16
#define CRC_LEN             2

#define INV_ADDR_LEN        8 // uint64_t
#define INV_INTERVAL_LEN    2 // uint16_t


#define MQTT_ADDR_LEN       4 // IP
#define MQTT_USER_LEN       16
#define MQTT_PWD_LEN        32
#define MQTT_TOPIC_LEN      32
#define MQTT_INTERVAL_LEN   2 // uint16_t


#define ADDR_START          0
#define ADDR_SSID           ADDR_START
#define ADDR_PWD            ADDR_SSID          + SSID_LEN
#define ADDR_DEVNAME        ADDR_PWD           + PWD_LEN
#define ADDR_INV0_ADDR      ADDR_DEVNAME       + DEVNAME_LEN
#define ADDR_INV_INTERVAL   ADDR_INV0_ADDR     + INV_ADDR_LEN

#define ADDR_MQTT_ADDR      ADDR_INV_INTERVAL  + INV_INTERVAL_LEN
#define ADDR_MQTT_USER      ADDR_MQTT_ADDR     + MQTT_ADDR_LEN
#define ADDR_MQTT_PWD       ADDR_MQTT_USER     + MQTT_USER_LEN
#define ADDR_MQTT_TOPIC     ADDR_MQTT_PWD      + MQTT_PWD_LEN
#define ADDR_MQTT_INTERVAL  ADDR_MQTT_TOPIC    + MQTT_TOPIC_LEN

#define ADDR_NEXT           ADDR_MQTT_INTERVAL + MQTT_INTERVAL_LEN

#define ADDR_SETTINGS_CRC   400

#if(ADDR_SETTINGS_CRC <= ADDR_NEXT)
#error address overlap!
#endif

#endif /*__DEFINES_H__*/
