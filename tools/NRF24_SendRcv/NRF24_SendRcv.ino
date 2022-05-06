#include <Arduino.h>
#include <SPI.h>
#include "CircularBuffer.h"
#include <RF24.h>
#include <RF24_config.h>
#include "hm_crc.h"
#include "hm_packets.h"

#include "Settings.h"     // Header für Einstellungen

#include "Debug.h"

#ifdef ESP8266
  #define DISABLE_EINT noInterrupts()
  #define ENABLE_EINT  interrupts()
#else     // für AVR z.B. ProMini oder Nano
  #define DISABLE_EINT EIMSK = 0x00
  #define ENABLE_EINT EIMSK = 0x01
#endif


#define RF_MAX_ADDR_WIDTH       (5) 
#define MAX_RF_PAYLOAD_SIZE     (32)

#ifdef ESP8266
#define PACKET_BUFFER_SIZE      (30) 
#else
#define PACKET_BUFFER_SIZE      (20) 
#endif

// Startup defaults until user reconfigures it
#define DEFAULT_RECV_CHANNEL    (3)             // 3 = Default channel for Hoymiles
//#define DEFAULT_SEND_CHANNEL  (75)            // 40 = Default channel for Hoymiles, 61
#define DEFAULT_RF_DATARATE     (RF24_250KBPS)  // Datarate

#include "NRF24_sniff_types.h"

static HM_Packets     hmPackets;
static uint32_t       tickMillis;


// Set up nRF24L01 radio on SPI bus plus CE/CS pins
// If more than one RF24 unit is used the another CS pin than 10 must be used
// This pin is used hard coded in SPI library
static RF24 radio1 (RF1_CE_PIN, RF1_CS_PIN);

static NRF24_packet_t bufferData[PACKET_BUFFER_SIZE];

static CircularBuffer<NRF24_packet_t> packetBuffer(bufferData, sizeof(bufferData) / sizeof(bufferData[0]));

static Serial_header_t SerialHdr;

#define CHECKCRC  1
static uint16_t lastCRC;
static uint16_t crc;

uint8_t         channels[]            = {/*3,*/ 23, 40, 61, 75};   //{1, 3, 6, 9, 11, 23, 40, 61, 75}
uint8_t         channelIdx            = 1;                         // fange mit 40 an
uint8_t         DEFAULT_SEND_CHANNEL  = channels[channelIdx];      // = 40

static unsigned long timeLastPacket = millis();

// Function forward declaration
static void SendPacket(uint64_t dest, uint8_t *buf, uint8_t len);
char * getChannelName (uint8_t i);

static const int    ANZAHL_VALUES         = 16;
static float        VALUES[ANZAHL_VALUES] = {};
static const char   *CHANNEL_NAMES[ANZAHL_VALUES] 
   = {"P1.Udc", "P1.Idc", "P1.Pdc", "P2.Udc", "P2.Idc", "P2.Pdc", 
      "E-Woche", "E-Total", "E1-Tag", "E2-Tag", "Uac", "Freq.ac", "Pac", "E-heute", "Ipv", "WR-Temp"};
static const uint8_t DIVISOR[ANZAHL_VALUES] = {10,100,10,10,100,10,1,1,1,1,10,100,10,0,0,10};

static const char BLANK = ' ';

static boolean istTag = true;

char CHANNELNAME_BUFFER[15];

#ifdef ESP8266
  #include "wifi.h"
  #include "ModWebserver.h"
  #include "Sonne.h"
#endif

char * getChannelName (uint8_t i) {
//-------------------------------
  memset (CHANNELNAME_BUFFER, 0, sizeof(CHANNELNAME_BUFFER));
  strcpy (CHANNELNAME_BUFFER, CHANNEL_NAMES[i]); 
  //itoa (i, CHANNELNAME_BUFFER, 10);
  return CHANNELNAME_BUFFER;
}

inline static void dumpData(uint8_t *p, int len) {
//-----------------------------------------------
  while (len--){
    if (*p < 16)
      DEBUG_OUT.print(F("0"));
    DEBUG_OUT.print(*p++, HEX);
  }
  DEBUG_OUT.print(BLANK);
}


float extractValue2 (uint8_t *p, int divisor) {
//-------------------------------------------
  uint16_t b1 = *p++;
  return ((float) (b1 << 8) + *p) / (float) divisor;
}


float extractValue4 (uint8_t *p, int divisor) {
//-------------------------------------------
  uint32_t ret  = *p++;
  for (uint8_t i = 1; i <= 3; i++)
    ret = (ret << 8) + *p++;
  return (ret / divisor);
}

void outChannel (uint8_t i) {
//-------------------------
  DEBUG_OUT.print(getChannelName(i)); DEBUG_OUT.print(F("\t:")); DEBUG_OUT.print(VALUES[i]); DEBUG_OUT.println(BLANK);  
}


void analyse01 (uint8_t *p) {    // p zeigt auf 01 hinter 2. WR-Adr
//----------------------------------
  //uint16_t val;
  //DEBUG_OUT.print (F("analyse 01: "));
  p += 3;
  // PV1.U   PV1.I   PV1.P     PV2.U   PV2.I   PV2.P
  // [0.1V]  [0.01A] [.1W]     [0.1V]  [0.01A] [.1W]
  for (int i = 0; i < 6; i++) {
    VALUES[i] = extractValue2 (p,DIVISOR[i]);   p += 2;
    outChannel(i);
  }
/* 
  DEBUG_OUT.print(F("PV1.U:")); DEBUG_OUT.print(extractValue2(p,10));
  p += 2;
  DEBUG_OUT.print(F(" PV1.I:")); DEBUG_OUT.print(extractValue2(p,100));
  p += 2;
  DEBUG_OUT.print(F(" PV1.Pac:")); DEBUG_OUT.print(extractValue2(p,10));
  p += 2;
  DEBUG_OUT.print(F(" PV2.U:")); DEBUG_OUT.print(extractValue2(p,10));
  p += 2;
  DEBUG_OUT.print(F(" PV2.I:")); DEBUG_OUT.print(extractValue2(p,100));
  p += 2;
  DEBUG_OUT.print(F(" PV2.Pac:")); DEBUG_OUT.print(extractValue2(p,10));
*/
  DEBUG_OUT.println();
}


void analyse02 (uint8_t *p) {    // p zeigt auf 02 hinter 2. WR-Adr
//----------------------------------
  //uint16_t val;
  //DEBUG_OUT.print (F("analyse 02: "));
  // +11 = Spannung, +13 = Frequenz, +15 = Leistung
  //p += 11;
  p++;
  for (int i = 6; i < 13; i++) {
    if (i == 7) {
       VALUES[i] = extractValue4 (p,DIVISOR[i]);   
       p += 4;    
    } 
    else {
      VALUES[i] = extractValue2 (p,DIVISOR[i]);   
      p += 2;
    }
    outChannel(i);
  }
  VALUES[13] = VALUES[8] + VALUES[9];     // E-heute = P1+P2
  if (VALUES[10] > 0)
    VALUES[14] = VALUES[12] / VALUES[10];   // Ipv = Pac / Spannung
/*
  DEBUG_OUT.print(F("P Woche:")); DEBUG_OUT.print(extractValue2(p,1));
  p += 2;
  DEBUG_OUT.print(F(" P Total:")); DEBUG_OUT.print(extractValue4(p,1));
  p += 4;
  DEBUG_OUT.print(F(" P1 Tag:")); DEBUG_OUT.print(extractValue2(p,1));
  p += 2;
  DEBUG_OUT.print(F(" P2 Tag:")); DEBUG_OUT.print(extractValue2(p,1));
  p += 2;
  
  DEBUG_OUT.print(F(" Spannung:")); DEBUG_OUT.print(extractValue2(p,10));
  p += 2;
  DEBUG_OUT.print(F(" Freq.:")); DEBUG_OUT.print(extractValue2(p,100));
  p += 2;
  DEBUG_OUT.print(F(" Leist.:")); DEBUG_OUT.print(extractValue2(p,10));
*/
  DEBUG_OUT.println();
}


void analyse83 (uint8_t *p) {    // p zeigt auf 83 hinter 2. WR-Adr
//----------------------------------
  //uint16_t val;
  //DEBUG_OUT.print (F("++++++analyse 83:"));
  p += 7;
  VALUES[15] = extractValue2 (p,DIVISOR[15]);  
  outChannel(15);
  DEBUG_OUT.println();
}

void analyseWords (uint8_t *p) {    // p zeigt auf 01 hinter 2. WR-Adr
//----------------------------------
  //uint16_t val;
  DEBUG_OUT.print (F("analyse words:"));
  p++;
  for (int i = 0; i <12;i++) {
    DEBUG_OUT.print(extractValue2(p,1));
    DEBUG_OUT.print(BLANK);
    p++;
  }
  DEBUG_OUT.println();
}

void analyseLongs (uint8_t *p) {    // p zeigt auf 01 hinter 2. WR-Adr
//----------------------------------
  //uint16_t val;
  DEBUG_OUT.print (F("analyse words:"));
  p++;
  for (int i = 0; i <12;i++) {
    DEBUG_OUT.print(extractValue4(p,1));
    DEBUG_OUT.print(BLANK);
    p++;
  }
  DEBUG_OUT.println();
}


#ifdef ESP8266
IRAM_ATTR
#endif
void handleNrf1Irq() {
//-------------------------
  static uint8_t lostPacketCount = 0;
  uint8_t pipe;
  
  DISABLE_EINT;
  
  // Loop until RX buffer(s) contain no more packets.
  while (radio1.available(&pipe)) {
    if (!packetBuffer.full()) {
      NRF24_packet_t *p = packetBuffer.getFront();
      p->timestamp = micros(); // Micros does not increase in interrupt, but it can be used.
      p->packetsLost = lostPacketCount;
      uint8_t packetLen = radio1.getPayloadSize();
      if (packetLen > MAX_RF_PAYLOAD_SIZE)
        packetLen = MAX_RF_PAYLOAD_SIZE;

      radio1.read(p->packet, packetLen);
      packetBuffer.pushFront(p);
      lostPacketCount = 0;
    }
    else {
      // Buffer full. Increase lost packet counter.
      bool tx_ok, tx_fail, rx_ready;
      if (lostPacketCount < 255)
        lostPacketCount++;
      // Call 'whatHappened' to reset interrupt status.
      radio1.whatHappened(tx_ok, tx_fail, rx_ready);
      // Flush buffer to drop the packet.
      radio1.flush_rx();
    }
  }
  ENABLE_EINT;
}


static void activateConf(void) {
//-----------------------------
  radio1.setChannel(DEFAULT_RECV_CHANNEL);
  radio1.setDataRate(DEFAULT_RF_DATARATE);
  radio1.disableCRC();
  radio1.setAutoAck(0x00);
  radio1.setPayloadSize(MAX_RF_PAYLOAD_SIZE);
  radio1.setAddressWidth(5);
  radio1.openReadingPipe(1, DTU_RADIO_ID);

  // We want only RX irqs
  radio1.maskIRQ(true, true, false);

  // Use lo PA level, as a higher level will disturb CH340 DEBUG_OUT usb adapter
  radio1.setPALevel(RF24_PA_MAX);
  radio1.startListening();

  // Attach interrupt handler to NRF IRQ output. Overwrites any earlier handler.
  attachInterrupt(digitalPinToInterrupt(RF1_IRQ_PIN), handleNrf1Irq, FALLING); // NRF24 Irq pin is active low.

  // Initialize SerialHdr header's address member to promiscuous address.
  uint64_t addr = DTU_RADIO_ID;
  for (int8_t i = sizeof(SerialHdr.address) - 1; i >= 0; --i) {
    SerialHdr.address[i] = addr;
    addr >>= 8;
  }

#ifndef ESP8266
  DEBUG_OUT.println(F("\nRadio Config:"));
  radio1.printPrettyDetails();
  DEBUG_OUT.println();
#endif
  tickMillis = millis() + 200;
}


void setup(void) {
//--------------
  //Serial.begin(SER_BAUDRATE);
  DEBUG_OUT.begin(SER_BAUDRATE);
  DEBUG_OUT.flush();

  DEBUG_OUT.println(F("-- Hoymiles DTU Simulation --"));

  radio1.begin();

  // Disable shockburst for receiving and decode payload manually
  radio1.setAutoAck(false);
  radio1.setRetries(0, 0);

  // Configure nRF IRQ input
  pinMode(RF1_IRQ_PIN, INPUT);

  activateConf();

#ifdef ESP8266
  setupWifi();
  setupClock();
  setupWebServer();
  setupUpdateByOTA();
  calcSunUpDown (getNow());
  istTag = isDayTime();
  DEBUG_OUT.print ("Es ist "); DEBUG_OUT.println (istTag?"Tag":"Nacht");
  hmPackets.SetUnixTimeStamp (getNow());
#else
  hmPackets.SetUnixTimeStamp(0x62456430);
#endif
}

 uint8_t sendBuf[MAX_RF_PAYLOAD_SIZE];

void isTime2Send () {
//-----------------
  // Second timer

  if (millis() >= tickMillis) {
    static uint8_t tel = 0;
    tickMillis += 1000;    //200;
    //tickSec++; 
    hmPackets.UnixTimeStampTick();
/*    if (++tickSec >= 5) {   // 5
      hmPackets.UnixTimeStampTick();
      tickSec = 0;
    } */

    int32_t size = 0;
    uint64_t dest = WR1_RADIO_ID;

    if (tel > 5)
      tel = 0;
    
    if (tel == 0) {
      #ifdef ESP8266
      hmPackets.SetUnixTimeStamp (getNow());
      #endif
      size = hmPackets.GetTimePacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8);
    }
    else if (tel == 1)
      size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x81);
    else if (tel == 2)
      size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x80);
    else if (tel == 3) {
      size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x83);
      //tel = 0;
    }
    else if (tel == 4)
      size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x82);
    else if (tel == 5)
      size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x84);

    SendPacket(dest, (uint8_t *)&sendBuf, size);

    tel++;
    
/*    for (uint8_t warte = 0; warte < 2; warte++) {
      delay(1000);
      hmPackets.UnixTimeStampTick();
    }*/ 
  }
}


void outputPacket(NRF24_packet_t *p, uint8_t payloadLen) {
//-----------------------------------------------------

    // Write timestamp, packets lost, address and payload length
    //printf(" %09lu ", SerialHdr.timestamp);
    dumpData((uint8_t *)&SerialHdr.packetsLost, sizeof(SerialHdr.packetsLost));
    dumpData((uint8_t *)&SerialHdr.address, sizeof(SerialHdr.address));

    // Trailing bit?!?
    dumpData(&p->packet[0], 2);

    // Payload length from PCF
    dumpData(&payloadLen, sizeof(payloadLen));

    // Packet control field - PID Packet identification
    uint8_t val = (p->packet[1] >> 1) & 0x03;
    DEBUG_OUT.print(val);
    DEBUG_OUT.print(F("  "));

    if (payloadLen > 9) {
      dumpData(&p->packet[2], 1);
      dumpData(&p->packet[3], 4);
      dumpData(&p->packet[7], 4);
      
      uint16_t remain = payloadLen - 2 - 1 - 4 - 4 + 4;

      if (remain < 32) {
        dumpData(&p->packet[11], remain);
        printf_P(PSTR("%04X "), crc);

        if (((crc >> 8) != p->packet[payloadLen + 2]) || ((crc & 0xFF) != p->packet[payloadLen + 3]))
          DEBUG_OUT.print(0);
        else
          DEBUG_OUT.print(1);
      }
      else {
        DEBUG_OUT.print(F("Ill remain "));
        DEBUG_OUT.print(remain);
      }
    }
    else {
      dumpData(&p->packet[2], payloadLen + 2);
      printf_P(PSTR("%04X "), crc);
    }
    DEBUG_OUT.println();
}


void loop(void) {
//=============
  while (!packetBuffer.empty()) {
    timeLastPacket = millis();
    // One or more records present
    NRF24_packet_t *p = packetBuffer.getBack();

    // Shift payload data due to 9-bit packet control field
    for (int16_t j = sizeof(p->packet) - 1; j >= 0; j--) {
      if (j > 0)
        p->packet[j] = (byte)(p->packet[j] >> 7) | (byte)(p->packet[j - 1] << 1);
      else
        p->packet[j] = (byte)(p->packet[j] >> 7);
    }

    SerialHdr.timestamp   = p->timestamp;
    SerialHdr.packetsLost = p->packetsLost;

    // Check CRC
    crc = 0xFFFF;
    crc = crc16((uint8_t *)&SerialHdr.address, sizeof(SerialHdr.address), crc, 0, BYTES_TO_BITS(sizeof(SerialHdr.address)));
    // Payload length
    uint8_t payloadLen = ((p->packet[0] & 0x01) << 5) | (p->packet[1] >> 3);
    // Add one byte and one bit for 9-bit packet control field
    crc = crc16((uint8_t *)&p->packet[0], sizeof(p->packet), crc, 7, BYTES_TO_BITS(payloadLen + 1) + 1);

    if (CHECKCRC) {
      // If CRC is invalid only show lost packets
      if (((crc >> 8) != p->packet[payloadLen + 2]) || ((crc & 0xFF) != p->packet[payloadLen + 3])) {
        if (p->packetsLost > 0) {
          DEBUG_OUT.print(F(" Lost: "));
          DEBUG_OUT.println(p->packetsLost);
        }
        packetBuffer.popBack();
        continue;
      }

      // Dump a decoded packet only once
      if (lastCRC == crc) {
        packetBuffer.popBack();
        continue;
      }
      lastCRC = crc;
    }

    // Don't dump mysterious ack packages
    if (payloadLen == 0) {
        packetBuffer.popBack();
        continue;
    }

    #ifdef DEBUG
    outputPacket (p, payloadLen);
    #endif
    
    uint8_t cmd = p->packet[11];
    if (cmd == 0x02)
      analyse02 (&p->packet[11]);
    else if (cmd == 0x01)     
      analyse01 (&p->packet[11]); 
    //if (p->packet[11] == 0x83 || p->packet[11] == 0x82) analyse83 (&p->packet[11], payloadLen);
    else if (cmd == 0x03) {
      analyseWords (&p->packet[11]);
      analyseLongs (&p->packet[11]);
    }
    else if (cmd == 0x81)   // ???
      ;
    else if (cmd == 0x83) 
      analyse83 (&p->packet[11]); 
    else {
      DEBUG_OUT.print (F("---- neues cmd=")); DEBUG_OUT.println(cmd, HEX);
      analyseWords (&p->packet[11]);
      analyseLongs (&p->packet[11]);
    }
    if (p->packetsLost > 0) {
      DEBUG_OUT.print(F(" Lost: "));
      DEBUG_OUT.print(p->packetsLost);
    }
    DEBUG_OUT.println();

    #ifndef ESP8266
    for (uint8_t i = 0; i < ANZAHL_VALUES; i++) {
      //outChannel(i);
      Serial.print(getChannelName(i)); Serial.print(':'); Serial.print(VALUES[i]); Serial.println(BLANK);  // Schnittstelle bei Arduino
    }
    DEBUG_OUT.println();
    #endif
    
    // Remove record as we're done with it.
    packetBuffer.popBack();
  }

  if (istTag) 
    isTime2Send();

  #ifdef ESP8266
  checkWifi();
  webserverHandle();
  checkUpdateByOTA();
  if (hour() == 0 && minute() == 0) {
    calcSunUpDown(getNow());  
  }
  if (minute() % 15 == 0 && second () == 0) {  // alle 15 Minuten neu berechnen ob noch hell
    istTag = isDayTime();
    DEBUG_OUT.print ("Es ist "); DEBUG_OUT.println (istTag?"Tag":"Nacht");
  }
  #endif
/*
  if (millis() > timeLastPacket + 60UL*SECOND) {  // 60 Sekunden
    channelIdx++;
    if (channelIdx >= sizeof(channels)) channelIdx = 0;
    DEFAULT_SEND_CHANNEL = channels[channelIdx];
    DEBUG_OUT.print (F("\nneuer DEFAULT_SEND_CHANNEL: ")); DEBUG_OUT.println(DEFAULT_SEND_CHANNEL);
    timeLastPacket = millis();
  }
*/
}


static void SendPacket(uint64_t dest, uint8_t *buf, uint8_t len) {
//--------------------------------------------------------------
  DISABLE_EINT;
  radio1.stopListening();

#ifdef CHANNEL_HOP
  static uint8_t hop = 0;
  #if DEBUG_SEND    
  DEBUG_OUT.print(F("Send... CH"));
  DEBUG_OUT.println(channels[hop]);
  #endif  
  radio1.setChannel(channels[hop++]);
  if (hop >= sizeof(channels) / sizeof(channels[0]))
    hop = 0;
#else
  radio1.setChannel(DEFAULT_SEND_CHANNEL);
#endif

  radio1.openWritingPipe(dest);
  radio1.setCRCLength(RF24_CRC_16);
  radio1.enableDynamicPayloads();
  radio1.setAutoAck(true);
  radio1.setRetries(3, 15);

  radio1.write(buf, len);

  // Try to avoid zero payload acks (has no effect)
  radio1.openWritingPipe(DUMMY_RADIO_ID);

  radio1.setAutoAck(false);
  radio1.setRetries(0, 0);
  radio1.disableDynamicPayloads();
  radio1.setCRCLength(RF24_CRC_DISABLED);

  radio1.setChannel(DEFAULT_RECV_CHANNEL);
  radio1.startListening();
  ENABLE_EINT;
}
