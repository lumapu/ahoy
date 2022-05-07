#include <Arduino.h>
#include <SPI.h>
#include "CircularBuffer.h"
#include <RF24.h>
#include "printf.h"
#include <RF24_config.h>
#include "hm_crc.h"
#include "hm_packets.h"

#include "Settings.h"     // Header für Einstellungen
#include "Debug.h"
#include "Inverters.h"

const char VERSION[] PROGMEM = "0.1.6";


#ifdef ESP8266
  #define DISABLE_EINT noInterrupts()
  #define ENABLE_EINT  interrupts()
#else     // für AVR z.B. ProMini oder Nano
  #define DISABLE_EINT EIMSK = 0x00
  #define ENABLE_EINT EIMSK = 0x01
#endif


#ifdef ESP8266
#define PACKET_BUFFER_SIZE      (30) 
#else
#define PACKET_BUFFER_SIZE      (20) 
#endif

// Startup defaults until user reconfigures it
//#define DEFAULT_RECV_CHANNEL    (3)             // 3 = Default channel for Hoymiles
//#define DEFAULT_SEND_CHANNEL  (75)            // 40 = Default channel for Hoymiles, 61

static HM_Packets     hmPackets;
static uint32_t       tickMillis;

// Set up nRF24L01 radio on SPI bus plus CE/CS pins
// If more than one RF24 unit is used the another CS pin than 10 must be used
// This pin is used hard coded in SPI library
static RF24 Radio (RF1_CE_PIN, RF1_CS_PIN);

static NRF24_packet_t bufferData[PACKET_BUFFER_SIZE];

static CircularBuffer<NRF24_packet_t> packetBuffer(bufferData, sizeof(bufferData) / sizeof(bufferData[0]));

static Serial_header_t SerialHdr;

#define CHECKCRC  1
static uint16_t lastCRC;
static uint16_t crc;

uint8_t         channels[]            = {3, 23, 40, 61, 75};   //{1, 3, 6, 9, 11, 23, 40, 61, 75}
uint8_t         channelIdx            = 2;                         // fange mit 40 an
uint8_t         DEFAULT_SEND_CHANNEL  = channels[channelIdx];      // = 40

#if USE_POOR_MAN_CHANNEL_HOPPING_RCV
uint8_t         rcvChannelIdx         = 0; 
uint8_t         rcvChannels[]         = {3, 23, 40, 61, 75};   //{1, 3, 6, 9, 11, 23, 40, 61, 75}
uint8_t         DEFAULT_RECV_CHANNEL  = rcvChannels[rcvChannelIdx];      //3;
uint8_t         intvl = 4;          // Zeit für poor man hopping
int             hophop;
#else
uint8_t         DEFAULT_RECV_CHANNEL  = 3;
#endif

boolean         valueChanged          = false;

static unsigned long timeLastPacket = millis();
static unsigned long timeLastIstTagCheck =  millis();
static unsigned long timeLastRcvChannelSwitch = millis();

// Function forward declaration
static void SendPacket(uint64_t dest, uint8_t *buf, uint8_t len);


static const char BLANK = ' ';

static boolean istTag = true;

char CHANNELNAME_BUFFER[15];

#ifdef ESP8266
  #include "wifi.h"
  #include "ModWebserver.h"
  #include "Sonne.h"
#endif


inline static void dumpData(uint8_t *p, int len) {
//-----------------------------------------------
  while (len > 0){
    if (*p < 16)
      DEBUG_OUT.print(F("0"));
    DEBUG_OUT.print(*p++, HEX);
    len--;
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

void outChannel (uint8_t wr, uint8_t i) {
//------------------------------------
  DEBUG_OUT.print(getMeasureName(wr, i)); 
  DEBUG_OUT.print(F("\t:")); 
  DEBUG_OUT.print(getMeasureValue(wr,i)); 
  DEBUG_OUT.println(BLANK);  
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
  DEBUG_OUT.print (F("analyse longs:"));
  p++;
  for (int i = 0; i <12;i++) {
    DEBUG_OUT.print(extractValue4(p,1));
    DEBUG_OUT.print(BLANK);
    p++;
  }
  DEBUG_OUT.println();
}


void analyse (NRF24_packet_t *p) {
//------------------------------
  uint8_t wrIdx = findInverter (&p->packet[3]);
  //DEBUG_OUT.print ("wrIdx="); DEBUG_OUT.println (wrIdx);
  if (wrIdx == 0xFF) return;
  uint8_t cmd = p->packet[11];
  float val = 0;
  if (cmd == 0x01 || cmd == 0x02 || cmd == 0x83) {
    const measureDef_t *defs = inverters[wrIdx].measureDef;

    for (uint8_t i = 0; i < inverters[wrIdx].anzMeasures; i++) {
      if (defs[i].teleId == cmd) {
        uint8_t pos = defs[i].pos;
        if (defs[i].bytes == 2)  
          val = extractValue2 (&p->packet[pos], getDivisor(wrIdx, i) );
        else if (defs[i].bytes == 4)
          val = extractValue4 (&p->packet[pos], getDivisor(wrIdx, i) );
        valueChanged = valueChanged ||(val != inverters[wrIdx].values[i]);
        inverters[wrIdx].values[i] = val;
      }      
    }
    // calculated funstions
    for (uint8_t i = 0; i < inverters[wrIdx].anzMeasureCalculated; i++) {
      val = inverters[wrIdx].measureCalculated[i].f (inverters[wrIdx].values);
      int idx = inverters[wrIdx].anzMeasures + i;
      valueChanged = valueChanged ||(val != inverters[wrIdx].values[idx]);
      inverters[wrIdx].values[idx] = val;
    }
  }
  else if (cmd == 0x81) {
    ;
  }
  else {
    DEBUG_OUT.print (F("---- neues cmd=")); DEBUG_OUT.println(cmd, HEX);
    analyseWords (&p->packet[11]);
    analyseLongs (&p->packet[11]);
    DEBUG_OUT.println();
  }
  if (p->packetsLost > 0) {
    DEBUG_OUT.print(F(" Lost: "));
    DEBUG_OUT.println(p->packetsLost);
  }
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
  while (Radio.available(&pipe)) {
    if (!packetBuffer.full()) {
      NRF24_packet_t *p = packetBuffer.getFront();
      p->timestamp = micros(); // Micros does not increase in interrupt, but it can be used.
      p->packetsLost = lostPacketCount;
      p->rcvChannel  = DEFAULT_RECV_CHANNEL;
      uint8_t packetLen = Radio.getPayloadSize();
      if (packetLen > MAX_RF_PAYLOAD_SIZE)
        packetLen = MAX_RF_PAYLOAD_SIZE;

      Radio.read(p->packet, packetLen);
      packetBuffer.pushFront(p);
      lostPacketCount = 0;
    }
    else {
      // Buffer full. Increase lost packet counter.
      bool tx_ok, tx_fail, rx_ready;
      if (lostPacketCount < 255)
        lostPacketCount++;
      // Call 'whatHappened' to reset interrupt status.
      Radio.whatHappened(tx_ok, tx_fail, rx_ready);
      // Flush buffer to drop the packet.
      Radio.flush_rx();
    }
  }
  ENABLE_EINT;
}


static void activateConf(void) {
//-----------------------------
  Radio.begin();
  // Disable shockburst for receiving and decode payload manually
  Radio.setAutoAck(false);
  Radio.setRetries(0, 0);
  Radio.setChannel(DEFAULT_RECV_CHANNEL);
  Radio.setDataRate(DEFAULT_RF_DATARATE);
  Radio.disableCRC();
  Radio.setAutoAck(0x00);
  Radio.setPayloadSize(MAX_RF_PAYLOAD_SIZE);
  Radio.setAddressWidth(5);
  Radio.openReadingPipe(1, DTU_RADIO_ID);

  // We want only RX irqs
  Radio.maskIRQ(true, true, false);

  // Use lo PA level, as a higher level will disturb CH340 DEBUG_OUT usb adapter
  Radio.setPALevel(RF24_PA_MAX);
  Radio.startListening();

  // Attach interrupt handler to NRF IRQ output. Overwrites any earlier handler.
  attachInterrupt(digitalPinToInterrupt(RF1_IRQ_PIN), handleNrf1Irq, FALLING); // NRF24 Irq pin is active low.

  // Initialize SerialHdr header's address member to promiscuous address.
  uint64_t addr = DTU_RADIO_ID;
  for (int8_t i = sizeof(SerialHdr.address) - 1; i >= 0; --i) {
    SerialHdr.address[i] = addr;
    addr >>= 8;
  }

  //Radio.printDetails();
  //DEBUG_OUT.println();
  tickMillis = millis() + 200;
}

#define resetRF24() activateConf()


void setup(void) {
//--------------
  #ifndef DEBUG
  #ifndef ESP8266
  Serial.begin(SER_BAUDRATE);
  #endif
  #endif
  printf_begin();
  DEBUG_OUT.begin(SER_BAUDRATE);
  DEBUG_OUT.flush();

  DEBUG_OUT.println(F("-- Hoymiles DTU Simulation --"));

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
  DEBUG_OUT.print (F("Es ist ")); DEBUG_OUT.println (istTag?F("Tag"):F("Nacht"));
  hmPackets.SetUnixTimeStamp (getNow());
#else
  hmPackets.SetUnixTimeStamp(0x62456430);
#endif

  setupInverts();
}

 uint8_t sendBuf[MAX_RF_PAYLOAD_SIZE];

void isTime2Send () {
//-----------------
  // Second timer
  static const uint8_t warteZeit = 1;
  static uint8_t tickSec = 0;
  if (millis() >= tickMillis) {
    static uint8_t tel = 0;
    tickMillis += warteZeit*1000;    //200;
    tickSec++; 
   
    if (++tickSec >= 1) {   // 5
      for (uint8_t c=0; c < warteZeit; c++) hmPackets.UnixTimeStampTick();
      tickSec = 0;
    } 

    int32_t size = 0;
    uint64_t dest =  0;
    for (uint8_t wr = 0; wr < anzInv; wr++) {
      dest = inverters[wr].RadioId;

      if (tel > 1)
        tel = 0;
      
      if (tel == 0) {
        #ifdef ESP8266
        hmPackets.SetUnixTimeStamp (getNow());
        #endif
        size = hmPackets.GetTimePacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8);
        //DEBUG_OUT.print ("Timepacket mit cid="); DEBUG_OUT.println(sendBuf[10], HEX);
      }
      else if (tel <= 1) 
        size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15,  0x80 + tel - 1);

      SendPacket (dest, (uint8_t *)&sendBuf, size);
    }  // for wr

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
    char _buf[20];
    sprintf_P(_buf, PSTR("rcv CH:%d "), p->rcvChannel);
    DEBUG_OUT.print (_buf);
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
    DEBUG_OUT.flush();
}

void writeArduinoInterface() {
//--------------------------
  if (valueChanged) {
    for (uint8_t wr = 0; wr < anzInv; wr++) {
      if (anzInv > 1) {
        Serial.print(wr); Serial.print('.');
      }
      for (uint8_t i = 0; i < inverters[wr].anzTotalMeasures; i++) {
        Serial.print(getMeasureName(wr,i));    // Schnittstelle bei Arduino
        Serial.print('='); 
        Serial.print(getMeasureValue(wr,i), getDigits(wr,i));   // Schnittstelle bei Arduino
        Serial.print (BLANK);
        Serial.println (getUnit(wr, i));
      }  // for i
      
    }  // for wr
    Serial.println(F("-----------------------"));
    valueChanged = false;
  }
}

boolean doCheckCrc (NRF24_packet_t *p, uint8_t payloadLen) {
//--------------------------------------------------------
  crc = 0xFFFF;
  crc = crc16((uint8_t *)&SerialHdr.address, sizeof(SerialHdr.address), crc, 0, BYTES_TO_BITS(sizeof(SerialHdr.address)));
  // Payload length
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
      return false;
    }
  
    // Dump a decoded packet only once
    if (lastCRC == crc) {
      packetBuffer.popBack();
      return false;
    }
    lastCRC = crc;
  }
  
  // Don't dump mysterious ack packages
  if (payloadLen == 0) {
      packetBuffer.popBack();
      return false;
  }
  return true;
}

void poorManChannelHopping() {
//--------------------------
  if (hophop <= 0) return;
  if (millis() >= timeLastRcvChannelSwitch + intvl) {
    rcvChannelIdx++;
    if (rcvChannelIdx >= sizeof(rcvChannels))
      rcvChannelIdx = 0;
    DEFAULT_RECV_CHANNEL  = rcvChannels[rcvChannelIdx]; 
    DISABLE_EINT;
    Radio.stopListening();
    Radio.setChannel (DEFAULT_RECV_CHANNEL);
    Radio.startListening();
    ENABLE_EINT;      
    timeLastRcvChannelSwitch = millis();
    hophop--;
  }
  
}
void loop(void) {
//=============
  // poor man channel hopping on receive
#if USE_POOR_MAN_CHANNEL_HOPPING_RCV
  poorManChannelHopping();
#endif

  if (millis()  > timeLastPacket + 50000UL) {
    DEBUG_OUT.println (F("Reset RF24"));
    resetRF24();
    timeLastPacket = millis(); 
  }
  
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

    uint8_t payloadLen = ((p->packet[0] & 0x01) << 5) | (p->packet[1] >> 3);
    // Check CRC
    if (! doCheckCrc(p, payloadLen) )
      continue;

    #ifdef DEBUG
    uint8_t cmd = p->packet[11];
    //if (cmd != 0x01 && cmd != 0x02 && cmd != 0x83 && cmd != 0x81)
      outputPacket (p, payloadLen);
    #endif

    analyse (p);

    #ifndef ESP8266
    writeArduinoInterface();
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
    delay (60*1000);
  }

  if (millis() > timeLastIstTagCheck + 15UL * 60UL * 1000UL) {   // alle 15 Minuten neu berechnen ob noch hell
    istTag = isDayTime();
    DEBUG_OUT.print (F("Es ist ")); DEBUG_OUT.println (istTag?F("Tag"):F("Nacht"));
    timeLastIstTagCheck = millis();
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
  //DEBUG_OUT.print (F("Sende: ")); DEBUG_OUT.println (buf[9],  HEX);
  //dumpData (buf, len); DEBUG_OUT.println();
  DISABLE_EINT;
  Radio.stopListening();

#ifdef CHANNEL_HOP
  static uint8_t hop = 0;
  #if DEBUG_SEND    
  DEBUG_OUT.print(F("Send... CH"));
  DEBUG_OUT.println(channels[hop]);
  #endif  
  Radio.setChannel(channels[hop++]);
  if (hop >= sizeof(channels) / sizeof(channels[0]))
    hop = 0;
#else
  Radio.setChannel(DEFAULT_SEND_CHANNEL);
#endif

  Radio.openWritingPipe(dest);
  Radio.setCRCLength(RF24_CRC_16);
  Radio.enableDynamicPayloads();
  Radio.setAutoAck(true);
  Radio.setRetries(3, 15);

  bool res = Radio.write(buf, len);
  // Try to avoid zero payload acks (has no effect)
  Radio.openWritingPipe(DUMMY_RADIO_ID);

  Radio.setAutoAck(false);
  Radio.setRetries(0, 0);
  Radio.disableDynamicPayloads();
  Radio.setCRCLength(RF24_CRC_DISABLED);

  Radio.setChannel(DEFAULT_RECV_CHANNEL);
  Radio.startListening();
  ENABLE_EINT;
#if USE_POOR_MAN_CHANNEL_HOPPING_RCV
  hophop = 5 * sizeof(rcvChannels);
#endif
}
