#ifndef __SETTINGS_H
#define __SETTINGS_H

// Ausgabe von Debug Infos auf der seriellen Console
#define DEBUG
#define SER_BAUDRATE            (115200)

// Ausgabe was gesendet wird; 0 oder 1 
#define DEBUG_SEND  0   

// soll zwichen den Sendekanälen 23, 40, 61, 75 ständig gewechselt werden
#define CHANNEL_HOP

// mit OTA Support, also update der Firmware über WLan mittels IP/update
#define WITH_OTA

// Hardware configuration
#ifdef ESP8266
#define RF1_CE_PIN  (D4)
#define RF1_CS_PIN  (D8)
#define RF1_IRQ_PIN (D3)
#else
#define RF1_CE_PIN  (9)
#define RF1_CS_PIN  (10)
#define RF1_IRQ_PIN (2)
#endif

union longlongasbytes {
  uint64_t ull;
  uint8_t bytes[8];  
};


uint64_t Serial2RadioID (uint64_t sn) {   
//----------------------------------
  longlongasbytes llsn;
  longlongasbytes res;
  llsn.ull = sn;
  res.ull = 0;
  res.bytes[4] = llsn.bytes[0];
  res.bytes[3] = llsn.bytes[1];
  res.bytes[2] = llsn.bytes[2];
  res.bytes[1] = llsn.bytes[3];
  res.bytes[0] = 0x01;
  return res.ull;
}

// WR und DTU
#define DUMMY_RADIO_ID          ((uint64_t)0xDEADBEEF01ULL) 
#define SerialWR                0x114172607952ULL				// <<<<<<<<<<<<<<<<<<<<<<< anpassen
uint64_t WR1_RADIO_ID           = Serial2RadioID (SerialWR);    // ((uint64_t)0x5279607201ULL);          
#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)


// Webserver
#define WEBSERVER_PORT      80

// Time Server
//#define TIMESERVER_NAME "pool.ntp.org"
#define TIMESERVER_NAME "fritz.box"

#ifdef WITH_OTA
// OTA Einstellungen
#define UPDATESERVER_PORT   WEBSERVER_PORT+1			
#define UPDATESERVER_DIR    "/update"							// mittels IP:81/update kommt man dann auf die OTA-Seite
#define UPDATESERVER_USER   "username_für_OTA"					// <<<<<<<<<<<<<<<<<<<<<<< anpassen
#define UPDATESERVER_PW     "passwort_für_OTA"					// <<<<<<<<<<<<<<<<<<<<<<< anpassen
#endif

// internes WLan
// PREFIXE dienen dazu, die eigenen WLans (wenn mehrere) vonfremden zu unterscheiden
// gehe hier davon aus, dass alle WLans das gleiche Passwort haben. Wenn nicht, dann mehre Passwörter hinterlegen
#define SSID_PREFIX1         "wlan1-Prefix"						// <<<<<<<<<<<<<<<<<<<<<<< anpassen
#define SSID_PREFIX2         "wlan2-Prefix"						// <<<<<<<<<<<<<<<<<<<<<<< anpassen
#define SSID_PASSWORD        "wlan-passwort"					// <<<<<<<<<<<<<<<<<<<<<<< anpassen

// zur Berechnung von Sonnenauf- und -untergang
#define  geoBreite  49.2866
#define  geoLaenge  7.3416


#endif
