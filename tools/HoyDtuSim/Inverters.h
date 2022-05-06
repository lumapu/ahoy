#ifndef __INVERTERS_H
#define __INVERTERS_H

// Ausgabe von Debug Infos auf der seriellen Console

#include "Settings.h"
#include "Debug.h"


typedef struct _NRF24_packet_t {
  uint32_t timestamp;
  uint8_t  packetsLost;
  uint8_t  rcvChannel;
  uint8_t  packet[MAX_RF_PAYLOAD_SIZE];
} NRF24_packet_t;


typedef struct _Serial_header_t {
  unsigned long timestamp;
  uint8_t  packetsLost;
  uint8_t  address[RF_MAX_ADDR_WIDTH];    // MSB first, always RF_MAX_ADDR_WIDTH bytes.
} Serial_header_t;


// structs für Inverter und Kanalwerte

// Liste der Einheiten
enum UNITS {UNIT_V = 0, UNIT_HZ, UNIT_A, UNIT_W,  UNIT_WH, UNIT_C, UNIT_KWH, UNIT_MA, UNIT_PCT};
const char* const units[] = {"V", "Hz", "A", "W", "Wh", "°C", "KWh", "mA", "%"};

// CH0 is default channel (freq, ac, temp)
enum CHANNELS {CH0 = 0, CH1, CH2, CH3, CH4};
enum CMDS     {CMD01 = 0x01, CMD02, CMD03, CMD83 = 0x83, CMD84};
enum DIVS     {DIV1 = 0, DIV10, DIV100, DIV1000};

#define BYTES2        2
#define BYTES4        4

const char UDC[]      PROGMEM = "Udc";
const char IDC[]      PROGMEM = "Idc";
const char PDC[]      PROGMEM = "Pdc";
const char E_WOCHE[]  PROGMEM = "E-Woche";
const char E_TOTAL[]  PROGMEM = "E-Total";
const char E_TAG[]    PROGMEM = "E-Tag";
const char UAC[]      PROGMEM = "Uac";
const char FREQ[]     PROGMEM = "Freq.ac";
const char PAC[]      PROGMEM = "Pac";
const char E_HEUTE[]  PROGMEM  = "E-heute";
const char IPV[]      PROGMEM  = "Ipv";
const char WR_TEMP[]  PROGMEM = "WR-Temp";
const char PERCNT[]   PROGMEM = "Pct";

#define IDX_UDC       0
#define IDX_IDC       1
#define IDX_PDC       2
#define IDX_E_WOCHE   3
#define IDX_E_TOTAL   4
#define IDX_E_TAG     5
#define IDX_UAC       6
#define IDX_FREQ      7
#define IDX_PAC       8
#define IDX_E_HEUTE   9
#define IDX_IPV      10
#define IDX_WR_TEMP  11
#define IDX_PERCNT   12    

const char* const NAMES[] 
  = {UDC, IDC, PDC, E_WOCHE, E_TOTAL, E_TAG, UAC, FREQ, PAC, E_HEUTE, IPV, WR_TEMP, PERCNT};

typedef float (*calcValueFunc)(float *);

struct measureDef_t {
  uint8_t     nameIdx;        //const char* name;           // Zeiger auf den Messwertnamen
  uint8_t     channel;        // 0..4, 
  uint8_t     unitIdx;        // Index in die Liste der Einheiten 'units'
  uint8_t     teleId;         // Telegramm ID, das was hinter der 2. WR Nummer im Telegramm, 02, 03, 83
  uint8_t     pos;            // ab dieser POsition beginnt der Wert (Big Endian)
  uint8_t     bytes;          // Anzahl der Bytes
  uint8_t     digits;
};

struct measureCalc_t {
  uint8_t     nameIdx;        //const char* name;           // Zeiger auf den Messwertnamen
  uint8_t     unitIdx;        // Index in die Liste der Einheiten 'units'
  uint8_t     digits;
  calcValueFunc f;            // die Funktion zur Berechnung von Werten, zb Summe von Werten
};


struct inverter_t {
  uint8_t       ID;                       // Inverter-ID = Index
  char          name[20];                 // Name des Inverters zb HM-600.1
  uint64_t      serialNo;                 // dier Seriennummer wie im Barcode auf dem WR, also 1141.....
  uint64_t      RadioId;                  // die gespiegelte (letzte 4 "Bytes") der Seriennummer
  const measureDef_t  *measureDef;              // aus Include HMxxx.h : Liste mit Definitionen der Messwerte, wie Telgramm, offset, länge, ...
  uint8_t       anzMeasures;              // Länge der Liste
  measureCalc_t *measureCalculated;       // Liste mit Defintion für berechnete Werte
  uint8_t       anzMeasureCalculated;     // Länge der Liste
  uint8_t       anzTotalMeasures;         // Gesamtanzahl Messwerte
  float         values[MAX_MEASURE_PER_INV];  // DIE Messewerte
};


char _buffer[20];

uint8_t anzInv = 0;
inverter_t inverters[MAX_ANZ_INV];
  
union longlongasbytes {
  uint64_t ull;
  uint32_t ul[2];
  uint8_t bytes[8];  
};

char *uint64toa (uint64_t s) {
//--------------------------------  
//0x1141 72607952ULL   
  sprintf(_buffer, "%lX%08lX", (unsigned long)(s>>32), (unsigned long)(s&0xFFFFFFFFULL));
  return _buffer;
}


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


void addInverter (uint8_t _ID, const char * _name, uint64_t _serial, 
                  const measureDef_t * liste, int anzMeasure,
                  measureCalc_t * calcs, int anzMeasureCalculated) {
//-------------------------------------------------------------------------------------
  if (anzInv >= MAX_ANZ_INV) {
    DEBUG_OUT.println(F("ANZ_INV zu klein!"));
    return;
  }
  inverter_t *p = &(inverters[anzInv]);
  p->ID                   = _ID;
  strcpy (p->name, _name);
  p->serialNo             = _serial;
  p->RadioId              = Serial2RadioID(_serial);
  p->measureDef           = liste;
  p->anzMeasures          = anzMeasure;
  p->anzMeasureCalculated = anzMeasureCalculated;
  p->measureCalculated    = calcs;
  p->anzTotalMeasures     = anzMeasure + anzMeasureCalculated;
  memset (p->values, 0, sizeof(p->values));

  DEBUG_OUT.print (F("WR       : "));      DEBUG_OUT.println(anzInv);
  DEBUG_OUT.print (F("Type     : "));      DEBUG_OUT.println(_name);
  DEBUG_OUT.print (F("Serial   : "));      DEBUG_OUT.println(uint64toa(_serial));
  DEBUG_OUT.print (F("Radio-ID : "));      DEBUG_OUT.println(uint64toa(p->RadioId));

  anzInv++;
}


static uint8_t toggle = 0;           // nur für Test, ob's auch für mehere WR funzt
uint8_t findInverter (uint8_t *fourbytes) {
//---------------------------------------
  for (uint8_t i = 0; i < anzInv; i++) {
    longlongasbytes llb;
    llb.ull = inverters[i].serialNo;  
    if (llb.bytes[3] == fourbytes[0] && 
        llb.bytes[2] == fourbytes[1] && 
        llb.bytes[1] == fourbytes[2] &&
        llb.bytes[0] == fourbytes[3] )
      {
        return i;
        //if (toggle) toggle = 0; else toggle = 1; return toggle;     // Test ob mehr WR  auch geht
      }
  }
  return 0xFF;      // nicht gefunden
}


char * error = {"error"};

char *getMeasureName (uint8_t wr, uint8_t i){
//------------------------------------------
  inverter_t *p = &(inverters[wr]);
  if (i >= p->anzTotalMeasures) return error;
  uint8_t idx, channel = 0;
  if (i < p->anzMeasures) {
    idx = p->measureDef[i].nameIdx;  
    channel = p->measureDef[i].channel;
  }
  else {
    idx = p->measureCalculated[i - p->anzMeasures].nameIdx;  
  }
  char tmp[20];
  strcpy_P (_buffer, NAMES[idx]);
  if (channel) {
    sprintf_P (tmp, PSTR(".CH%d"), channel);
    strcat(_buffer,tmp);
  }
  return _buffer;
}

const char *getUnit (uint8_t wr, uint8_t i) {
//------------------------------------------
  inverter_t *p = &(inverters[wr]);
  if (i >= p->anzTotalMeasures) return error;
  uint8_t idx;
  if (i < p->anzMeasures)
    idx = p->measureDef[i].unitIdx;
  else
    idx = p->measureCalculated[i-p->anzMeasures].unitIdx;
  
  //strcpy (_buffer, units[i]);
  //return _buffer;
  return units[idx];
}


float getMeasureValue (uint8_t wr, uint8_t i) {
//------------------------------------------
  if (i >= inverters[wr].anzTotalMeasures) return 0.0;
  return inverters[wr].values[i];
}


int getDivisor (uint8_t wr, uint8_t i) {
//------------------------------------
  inverter_t *p = &(inverters[wr]);
  if (i >= p->anzTotalMeasures) return 1;
  if (i < p->anzMeasures) {
    uint8_t digits = p->measureDef[i].digits;
    if (digits == DIV1) return 1;
    if (digits == DIV10) return 10;
    if (digits == DIV100) return 100;
    if (digits == DIV1000) return 1000;
    return 1;
  }
  else
    return p->measureCalculated[i].digits;  
}


uint8_t getDigits (uint8_t wr, uint8_t i) {
//---------------------------------------  
  inverter_t *p = &(inverters[wr]);
  if (i >= p->anzTotalMeasures) return 0;
  if (i < p->anzMeasures) 
    return p->measureDef[i].digits;
  else
    return p->measureCalculated[i-p->anzMeasures].digits;  
}

// +++++++++++++++++++++++++++++++++++    Inverter    ++++++++++++++++++++++++++++++++++++++++++++++

#include "HM600.h"            // für HM-600 und HM-700

#include "HM1200.h"

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


void setupInverts() {
//-----------------  

  addInverter (0,"HM-600", 0x114172607952ULL, 
               hm600_measureDef, HM600_MEASURE_LIST_LEN,     // Tabelle der Messwerte
               hm600_measureCalc, HM600_CALCED_LIST_LEN); // Tabelle berechnete Werte

/*
  addInverter (1,"HM-1200", 0x114172607952ULL, 
               hm1200_measureDef, HM1200_MEASURE_LIST_LEN,     // Tabelle der Messwerte
               hm1200_measureCalc, HM1200_CALCED_LIST_LEN);    // Tabelle berechnete Werte
*/
}


#endif
