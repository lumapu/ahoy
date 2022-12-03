//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>


#if defined(ESP8266)
    // for esp8266 environment
    #define DEF_DEVICE_NAME "\n\n +++ AHOY-UL ESP8266 +++"
    #define DEF_RF24_CS_PIN              15
    #define DEF_RF24_CE_PIN              2
    #define DEF_RF24_IRQ_PIN             0
#else
    // default pinout (GPIO Number) for Arduino Nano 328p
    #define DEF_DEVICE_NAME "\n\n +++ AHOY-UL A-NANO +++"
    #define DEF_RF24_CE_PIN         (4)
    #define DEF_RF24_CS_PIN         (10)
    #define DEF_RF24_IRQ_PIN        (3)
#endif


// default radio ID
#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)

// real inverter ID is taken from config_override.h at the bottom of this file
#define IV1_RADIO_ID ((uint64_t) 0x114144332211ULL)     // 0x1141 is type-id for HM800, lower 4bytes must be filled with real ID from INV-plate

// default NRF24 power, possible values (0 - 3)
#define DEF_AMPLIFIERPOWER      2

// number of packets hold in buffer
#define PACKET_BUFFER_SIZE     7

// number of configurable inverters
#define MAX_NUM_INVERTERS       2

// default serial interval
#define SERIAL_INTERVAL         5

// default send interval
#define SEND_INTERVAL           60                      //send interval if Rx OK
#define SEND_INTERVAL_MIN       10                      //send interval if no RX (used initial sync or signal loss), todo: shall be disabled over night

// maximum human readable inverter name length
#define MAX_NAME_LENGTH         16

// maximum buffer length of packet received / sent to RF24 module
#define MAX_RF_PAYLOAD_SIZE     32

// maximum total payload buffers (must be greater than the number of received frame fragments)
#define MAX_PAYLOAD_ENTRIES     5

// maximum requests for retransmits per payload (per inverter)
#define DEF_MAX_RETRANS_PER_PYLD    10

// number of seconds since last successful response, before inverter is marked inactive
#define INACT_THRES_SEC         300

// threshold of minimum power on which the inverter is marked as inactive
#define INACT_PWR_THRESH        3


//will be used for serial utils buffers
#define MAX_SERBYTES 120                            //serial input buffer length of this app
#define MAX_STRING_LEN 50                           //max length of output string at once
#define MAX_SER_PARAM 5                             //max num of serial cmd parameter


#if __has_include("config_override.h")
    #include "config_override.h"
#endif

#endif /*__CONFIG_H__*/
