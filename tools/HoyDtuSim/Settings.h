#ifndef __SETTINGS_H
#define __SETTINGS_H

// Ausgabe von Debug Infos auf der seriellen Console
#define DEBUG
#define SER_BAUDRATE            (115200)

#include "Debug.h"

// Ausgabe was gesendet wird; 0 oder 1 
#define DEBUG_SEND  0   

// soll zwichen den Sendekanälen 23, 40, 61, 75 ständig gewechselt werden
#define CHANNEL_HOP

// mit OTA Support, also update der Firmware über WLan mittels IP/update
#define WITH_OTA

// Hardware configuration
#ifdef ESP8266
#define RF1_CE_PIN (D4)
#define RF1_CS_PIN (D8)
#define RF1_IRQ_PIN (D3)
#else
#define RF1_CE_PIN (9)
#define RF1_CS_PIN (10)
#define RF1_IRQ_PIN (2)
#endif

// WR und DTU
#define RF_MAX_ADDR_WIDTH       (5) 
#define MAX_RF_PAYLOAD_SIZE     (32)
#define DEFAULT_RF_DATARATE     (RF24_250KBPS)  // Datarate

#define USE_POOR_MAN_CHANNEL_HOPPING_RCV  1     // 0 = not use

#define DUMMY_RADIO_ID          ((uint64_t)0xDEADBEEF01ULL) 
#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)
#define MAX_ANZ_INV 2             							// <<<<<<<<<<<<<<<<<<<<<<<< anpassen
#define MAX_MEASURE_PER_INV 25    // hier statisch, könnte auch dynamisch erzeugt werden, aber Overhead für dyn. Speicher?

// Webserver
#define WEBSERVER_PORT          80

// Time Server
//#define TIMESERVER_NAME "pool.ntp.org"
#define TIMESERVER_NAME "fritz.box"							// <<<<<<<<<<<<<<<<<<<<<<<< anpassen

#ifdef WITH_OTA
// OTA Einstellungen
#define UPDATESERVER_PORT   WEBSERVER_PORT+1
#define UPDATESERVER_DIR    "/update"
#define UPDATESERVER_USER   "?????"							// <<<<<<<<<<<<<<<<<<<<<<<< anpassen
#define UPDATESERVER_PW     "?????"							// <<<<<<<<<<<<<<<<<<<<<<<< anpassen
#endif

// internes WLan
// PREFIXE dienen dazu, die eigenen WLans (wenn mehrere) von fremden zu unterscheiden
// gehe hier davon aus, dass alle WLans das gleiche Passwort haben. Wenn nicht, dann mehre Passwörter hinterlegen
#define SSID_PREFIX1         "pre1"							// <<<<<<<<<<<<<<<<<<<<<<<< anpassen
#define SSID_PREFIX2         "pre2"							// <<<<<<<<<<<<<<<<<<<<<<<< anpassen
#define SSID_PASSWORD        "?????????????????"			// <<<<<<<<<<<<<<<<<<<<<<<< anpassen

// zur Berechnung von Sonnenauf- und -untergang
#define  geoBreite  49.2866									// <<<<<<<<<<<<<<<<<<<<<<<< anpassen
#define  geoLaenge  7.3416									// <<<<<<<<<<<<<<<<<<<<<<<< anpassen


#endif
