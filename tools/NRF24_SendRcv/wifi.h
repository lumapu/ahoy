#ifndef __WIFI_H
#define __WIFI_H

#include "Settings.h"
#include "Debug.h"
#include <ESP8266WiFi.h>
#include <Pinger.h>       // von url=https://www.technologytourist.com   

String SSID = "";         // bestes WLan

// Prototypes
time_t getNow ();
boolean setupWifi ();
boolean checkWifi();


String findWifi () {
//----------------
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
  uint8_t* bssid;
  int32_t channel;
  bool hidden;
  int scanResult;
  
  String best_ssid = "";
  int32_t best_rssi = -100;
  
  DEBUG_OUT.println(F("Starting WiFi scan..."));

  scanResult = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);

  if (scanResult == 0) {
    DEBUG_OUT.println(F("keine WLans"));
  } else if (scanResult > 0) {
    DEBUG_OUT.printf(PSTR("%d WLans gefunden:\n"), scanResult);

    // Print unsorted scan results
    for (int8_t i = 0; i < scanResult; i++) {
      WiFi.getNetworkInfo(i, ssid, encryptionType, rssi, bssid, channel, hidden);

      DEBUG_OUT.printf(PSTR("  %02d: [CH %02d] [%02X:%02X:%02X:%02X:%02X:%02X] %ddBm %c %c %s\n"),
                    i,
                    channel,
                    bssid[0], bssid[1], bssid[2],
                    bssid[3], bssid[4], bssid[5],
                    rssi,
                    (encryptionType == ENC_TYPE_NONE) ? ' ' : '*',
                    hidden ? 'H' : 'V',
                    ssid.c_str());
      delay(1);
      boolean check;
      #ifdef SSID_PREFIX1
      check = ssid.substring(0,strlen(SSID_PREFIX1)).equals(SSID_PREFIX1);
      #else
      check = true;
      #endif    
      #ifdef SSID_PREFIX2
      check = check || ssid.substring(0,strlen(SSID_PREFIX2)).equals(SSID_PREFIX2);
      #endif
      if (check) {
        if (rssi > best_rssi) {
          best_rssi = rssi;
          best_ssid = ssid;
        }
      }
    }
  } else {
    DEBUG_OUT.printf(PSTR("WiFi scan error %d"), scanResult);
  }

  if (! best_ssid.equals("")) {
    SSID = best_ssid;
    DEBUG_OUT.printf ("Bestes Wifi unter: %s\n", SSID.c_str());
    return SSID;
  }
  else
    return "";
}

void IP2string (IPAddress IP, char * buf) {
  sprintf (buf, "%d.%d.%d.%d", IP[0], IP[1], IP[2], IP[3]);
}

void connectWifi() {
//------------------
//  if (SSID.equals(""))
   String s = findWifi();

  if (!SSID.equals("")) {
    DEBUG_OUT.print("versuche zu verbinden mit "); DEBUG_OUT.println(SSID);
    //while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin (SSID, SSID_PASSWORD);
    int versuche = 20;
    while (WiFi.status() != WL_CONNECTED && versuche > 0) {
      delay(1000);
      versuche--;
      DEBUG_OUT.print(versuche); DEBUG_OUT.print(' ');
    }
    //}
    if (WiFi.status() == WL_CONNECTED) {
      char buffer[30];
      IP2string (WiFi.localIP(), buffer);
      String out = "\n[WiFi]Verbunden; meine IP:" + String (buffer);
      DEBUG_OUT.println (out);
    }
    else
      DEBUG_OUT.print("\nkeine Verbindung mit SSID "); DEBUG_OUT.println(SSID);
  }
}


boolean setupWifi () {
//------------------  
  int count=5;
  while (count-- && WiFi.status() != WL_CONNECTED)
    connectWifi();   
  return (WiFi.status() == WL_CONNECTED);
}


Pinger pinger;
IPAddress ROUTER = IPAddress(192,168,1,1);

boolean checkWifi() {
//---------------
  boolean NotConnected = (WiFi.status() != WL_CONNECTED) || !pinger.Ping(ROUTER);
  if (NotConnected) {
    setupWifi();
    if (WiFi.status() == WL_CONNECTED) 
      getNow();
  }
  return (WiFi.status() == WL_CONNECTED);
}



// ################  Clock  #################

#include <WiFiUdp.h>
#include <TimeLib.h>

IPAddress     timeServer;
unsigned int  localPort = 8888;
const int     NTP_PACKET_SIZE= 48;            // NTP time stamp is in the first 48 bytes of the message
byte          packetBuf[NTP_PACKET_SIZE];  // Buffer to hold incoming and outgoing packets
const int     timeZone = 1;                   // Central European Time = +1
long          SYNCINTERVALL = 0;
WiFiUDP Udp;                                  // A UDP instance to let us send and receive packets over UDP

// prototypes
time_t  getNtpTime ();
void    sendNTPpacket (IPAddress &address);
time_t  getNow ();
char*   getDateTimeStr (time_t no = getNow());
time_t  offsetDayLightSaving (uint32_t local_t);
bool    isDayofDaylightChange (time_t local_t);


void _setSyncInterval (long intervall) {
//----------------------------------------  
  SYNCINTERVALL = intervall;
  setSyncInterval (intervall);
}

void setupClock() {
//-----------------
  WiFi.hostByName (TIMESERVER_NAME,timeServer); // at this point the function works

  Udp.begin(localPort);

  getNtpTime();

  setSyncProvider (getNtpTime);
  while(timeStatus()== timeNotSet)
     delay(1); //

  _setSyncInterval (SECS_PER_DAY / 2); // Set seconds between re-sync

  //lastClock = now();
  //Serial.print("[NTP] get time from NTP server ");
  getNow();
  //char buf[20];
  DEBUG_OUT.print ("[NTP] get time from NTP server ");
  DEBUG_OUT.print (timeServer);
  //sprintf (buf, ": %02d:%02d:%02d", hour(no), minute(no), second(no));
  DEBUG_OUT.print (": got ");
  DEBUG_OUT.println (getDateTimeStr());
}

//*-------- NTP code ----------*/


time_t getNtpTime() {
//-------------------
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  //uint32_t beginWait = millis();
  //while (millis() - beginWait < 1500) {
  int versuch = 0;
  while (versuch < 5) {
    int wait = 150;      // results in max 1500 ms waitTime
    while (wait--) {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        //Serial.println("Receive NTP Response");
        Udp.read(packetBuf, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuf[40] << 24;
        secsSince1900 |= (unsigned long)packetBuf[41] << 16;
        secsSince1900 |= (unsigned long)packetBuf[42] << 8;
        secsSince1900 |= (unsigned long)packetBuf[43];
        // time_t now = secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
  
        time_t utc = secsSince1900 - 2208988800UL;
        time_t now = utc + (timeZone +offsetDayLightSaving(utc)) * SECS_PER_HOUR;
        
        if (isDayofDaylightChange (utc) && hour(utc) <= 4)
          _setSyncInterval (SECS_PER_HOUR);
        else
          _setSyncInterval (SECS_PER_DAY / 2);
       
        return now;
      }
      else
        delay(10);
    }
    versuch++;
  }
  return 0;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
//------------------------------------
  memset(packetBuf, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  packetBuf[0] = B11100011;   // LI, Version, Mode
  packetBuf[1] = 0;     // Stratum
  packetBuf[2] = 6;     // Max Interval between messages in seconds
  packetBuf[3] = 0xEC;  // Clock Precision
  // bytes 4 - 11 are for Root Delay and Dispersion and were set to 0 by memset
  packetBuf[12]  = 49;  // four-byte reference ID identifying
  packetBuf[13]  = 0x4E;
  packetBuf[14]  = 49;
  packetBuf[15]  = 52;
  // send the packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuf,NTP_PACKET_SIZE);
  Udp.endPacket();

}

int getTimeTrials = 0;

bool isValidDateTime (time_t no) {
  return (year(no) > 2020 && year(no) < 2038);  
}

bool isDayofDaylightChange (time_t local_t) {
//-----------------------------------------
  int jahr  = year  (local_t);
  int monat = month (local_t);
  int tag   = day   (local_t);
  bool ret = ( (monat ==3 && tag == (31 - (5 * jahr /4 + 4) % 7)) || 
               (monat==10 && tag == (31 - (5 * jahr /4 + 1) % 7)));
  DEBUG_OUT.print ("isDayofDaylightChange="); DEBUG_OUT.println (ret); 
  return ret;
}

// calculates the daylight saving time for middle Europe. Input: Unixtime in UTC (!)
// übernommen von Jurs, see : https://forum.arduino.cc/index.php?topic=172044.msg1278536#msg1278536
time_t offsetDayLightSaving (uint32_t local_t) {
//--------------------------------------------
  int monat = month (local_t);
  if (monat < 3 || monat > 10) return 0; // no DSL in Jan, Feb, Nov, Dez
  if (monat > 3 && monat < 10) return 1; // DSL in Apr, May, Jun, Jul, Aug, Sep
  int jahr  = year (local_t);
  int std   = hour (local_t);
  //int tag   = day  (local_t);
  int stundenBisHeute = (std + 24 * day(local_t));
  if ( (monat == 3  && stundenBisHeute >= (1 + timeZone + 24 * (31 - (5 * jahr /4 + 4) % 7))) || 
       (monat == 10 && stundenBisHeute <  (1 + timeZone + 24 * (31 - (5 * jahr /4 + 1) % 7))) )
    return 1;
  else
    return 0;
  /*
  int stundenBisWechsel = (1 + 24 * (31 - (5 * year(local_t) / 4 + 4) % 7));
  if (monat == 3  && stundenBisHeute >= stundenBisWechsel || monat == 10 && stundenBisHeute <  stundenBisWechsel)
    return 1;
  else
    return 0;
    */
}


time_t getNow () {
//---------------
  time_t jetzt = now();
  while (!isValidDateTime(jetzt) && getTimeTrials < 10)  { // ungültig, max 10x probieren
    if (getTimeTrials) {
      //Serial.print (getTimeTrials);
      //Serial.println(". Versuch für getNtpTime");
    }
    jetzt = getNtpTime ();
    if (isValidDateTime(jetzt)) {
      setTime (jetzt);
      getTimeTrials = 0;
    }
    else
      getTimeTrials++;
  }
  //return jetzt + offsetDayLightSaving(jetzt)*SECS_PER_HOUR;
  return jetzt;
}


char _timestr[24];

char* getNowStr (time_t no = getNow()) {
//------------------------------------
  sprintf (_timestr, "%02d:%02d:%02d", hour(no), minute(no), second(no));
  return _timestr;  
}

char* getTimeStr (time_t no = getNow()) {
//------------------------------------
  return getNowStr (no);
}

char* getDateTimeStr (time_t no) {
//------------------------------
  sprintf (_timestr, "%04d-%02d-%02d+%02d:%02d:%02d", year(no), month(no), day(no), hour(no), minute(no), second(no));
  return _timestr;  
}

char* getDateStr (time_t no) {
//------------------------------
  sprintf (_timestr, "%04d-%02d-%02d", year(no), month(no), day(no));
  return _timestr;  
}


#endif
