#ifndef __DEFINES_H__
#define __DEFINES_H__


//-------------------------------------
// PINOUT
//-------------------------------------
#define RF24_IRQ_PIN        4
#define RF24_CE_PIN         5
#define RF24_CS_PIN         15


//-------------------------------------
// VERSION
//-------------------------------------
#define VERSION_MAJOR       0
#define VERSION_MINOR       1
#define VERSION_PATCH       7


//-------------------------------------
// EEPROM
//-------------------------------------
#define SSID_LEN            32
#define PWD_LEN             64
#define DEVNAME_LEN         32

#define HOY_ADDR_LEN        6


#define ADDR_SSID           0 // start address
#define ADDR_PWD            ADDR_SSID          + SSID_LEN
#define ADDR_DEVNAME        ADDR_PWD           + PWD_LEN

#define ADDR_HOY_ADDR       ADDR_DEVNAME       + DEVNAME_LEN

#endif /*__DEFINES_H__*/
