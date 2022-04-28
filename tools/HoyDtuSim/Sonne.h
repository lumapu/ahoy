#ifndef __SONNE_H
#define __SONNE_H

#include "Settings.h"
#include "Debug.h"


long SunDown, SunUp;

void calcSunUpDown (time_t date) {
    //SunUpDown res = new SunUpDown();
    boolean isSummerTime = false;   // TODO TimeZone.getDefault().inDaylightTime(new Date(date));
    
    //- Bogenmass
    double brad = geoBreite / 180.0 * PI;
    // - Höhe Sonne -50 Bogenmin.
    double h0 = -50.0 / 60.0 / 180.0 * PI;
    //- Deklination dek, Tag des Jahres d0
    int tage = 30 * month(date) - 30 + day(date); 
    double dek = 0.40954 * sin (0.0172 * (tage - 79.35));
    double zh1 = sin (h0) - sin (brad)  *  sin(dek);
    double zh2 = cos(brad) * cos(dek);
    double zd = 12*acos (zh1/zh2) / PI;
    double zgl = -0.1752 * sin (0.03343 * tage + 0.5474) - 0.134 * sin (0.018234 * tage - 0.1939);
    //-Sonnenuntergang
    double tsu = 12 + zd - zgl;
    double su = (tsu + (15.0 - geoLaenge) / 15.0); 
    int std = (int)su;
    int minute = (int) ((su - std)*60);
    if (isSummerTime) std++;
    SunDown = (100*std + minute) * 100;
    
    //- Sonnenaufgang
    double tsa = 12 - zd - zgl;
    double sa = (tsa + (15.0 - geoLaenge) /15.0); 
    std = (int) sa;
    minute = (int) ((sa - std)*60);
    if (isSummerTime) std++;
    SunUp = (100*std + minute) * 100;
    DEBUG_OUT.print(F("Sonnenaufgang  :")); DEBUG_OUT.println(SunUp);
    DEBUG_OUT.print(F("Sonnenuntergang:")); DEBUG_OUT.println(SunDown);
}

boolean isDayTime() {
//-----------------
// 900 = 15 Minuten, vor Sonnenaufgang und nach -untergang
  const int offset=60*15;
  time_t no = getNow();
  long jetztMinuteU = (100 * hour(no+offset) + minute(no+offset)) * 100;
  long jetztMinuteO = (100 * hour(no-offset) + minute(no-offset)) * 100;

  return ((jetztMinuteU >= SunUp) &&(jetztMinuteO <= SunDown));  
}

#endif
