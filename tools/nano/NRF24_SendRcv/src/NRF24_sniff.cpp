#include <Arduino.h>
#include <SPI.h>
#include <CircularBuffer.h>
#include <RF24.h>
#include <RF24_config.h>

#include "hm_crc.h"
#include "hm_packets.h"

// Macros
#define DISABLE_EINT EIMSK = 0x00
#define ENABLE_EINT EIMSK = 0x01

// Hardware configuration
#define RF1_CE_PIN (9)
#define RF1_CS_PIN (6)
#define RF1_IRQ_PIN (2)

#define LED_PIN_STATUS (A0)

#define RF_MAX_ADDR_WIDTH (5) // Maximum address width, in bytes. MySensors use 5 bytes for addressing, where lowest byte is for node addressing.
#define MAX_RF_PAYLOAD_SIZE (32)
#define SER_BAUDRATE (115200)
#define PACKET_BUFFER_SIZE (20) // Maximum number of packets that can be buffered between reception by NRF and transmission over serial port.

// Startup defaults until user reconfigures it
#define DEFAULT_RECV_CHANNEL (3)           // 3 = Default channel for Hoymiles
#define DEFAULT_SEND_CHANNEL (40)          // 40 = Default channel for Hoymiles
#define DEFAULT_RF_DATARATE (RF24_250KBPS) // Datarate

#define DUMMY_RADIO_ID ((uint64_t)0xDEADBEEF01ULL) 
#define WR1_RADIO_ID ((uint64_t)0x1946107301ULL) // 0x1946107300ULL = WR1
#define WR2_RADIO_ID ((uint64_t)0x3944107301ULL) // 0x3944107301ULL = WR2
#define DTU_RADIO_ID ((uint64_t)0x1234567801ULL)

#include "NRF24_sniff_types.h"

unsigned long previousMillis = 0; // will store last time LED was updated
const long interval = 250;        // interval at which to blink (milliseconds)
int ledState = LOW;               // ledState used to set the LED

static HM_Packets hmPackets;
static uint32_t tickMillis;
static uint16_t tickSec;

static uint8_t sendBuf[MAX_RF_PAYLOAD_SIZE];

// Set up nRF24L01 radio on SPI bus plus CE/CS pins
// If more than one RF24 unit is used the another CS pin than 10 must be used
// This pin is used hard coded in SPI library
static RF24 radio1(RF1_CE_PIN, RF1_CS_PIN);

static NRF24_packet_t bufferData[PACKET_BUFFER_SIZE];

static CircularBuffer<NRF24_packet_t> packetBuffer(bufferData, sizeof(bufferData) / sizeof(bufferData[0]));

static Serial_header_t serialHdr;

static uint16_t lastCRC;

static bool bOperate = true;
static uint8_t checkCRC = 1;

int channels[] = {3, 23, 40, 61, 75};

// Function forward declaration
static void DumpConfig();
static void SendPacket(uint64_t dest, uint8_t *buf, uint8_t len);

inline static void dumpData(uint8_t *p, int len)
{
  while (len--)
  {
    if (*p < 16)
      Serial.print(F("0"));
    Serial.print(*p++, HEX);
  }
  Serial.print(F(" "));
}

static void handleNrf1Irq()
{
  static uint8_t lostPacketCount = 0;
  uint8_t pipe;

  // Loop until RX buffer(s) contain no more packets.
  while (radio1.available(&pipe))
  {
    if (!packetBuffer.full())
    {
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
    else
    {
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
}

static void activateConf(void)
{
  // Match MySensors' channel & datarate
  radio1.setChannel(DEFAULT_RECV_CHANNEL);
  radio1.setDataRate(DEFAULT_RF_DATARATE);

  radio1.disableCRC();
  radio1.setAutoAck(0x00);

  radio1.setPayloadSize(MAX_RF_PAYLOAD_SIZE);

  // Configure listening pipe with the 'promiscuous' address and start listening
  radio1.setAddressWidth(5);
  radio1.openReadingPipe(1, DTU_RADIO_ID);

  // Wen wan't only RX irqs
  radio1.maskIRQ(true, true, false);

  // Use lo PA level, as a higher level will disturb CH340 serial usb adapter
  radio1.setPALevel(RF24_PA_LOW);

  radio1.startListening();

  // Attach interrupt handler to NRF IRQ output. Overwrites any earlier handler.
  attachInterrupt(digitalPinToInterrupt(RF1_IRQ_PIN), handleNrf1Irq, FALLING); // NRF24 Irq pin is active low.

  // Initialize serial header's address member to promiscuous address.
  uint64_t addr = DTU_RADIO_ID;
  for (int8_t i = sizeof(serialHdr.address) - 1; i >= 0; --i)
  {
    serialHdr.address[i] = addr;
    addr >>= 8;
  }

  DumpConfig();

  tickMillis = millis() + 200;
}

static void DumpConfig()
{
  Serial.println(F("\nRadio 1:"));
  radio1.printDetails();

  Serial.println("");
}

static void DumpMenu()
{
  Serial.println(F("\n\nConfiguration:"));
  Serial.println(F("  d: Dump current configuration"));
  Serial.println(F("  f: Filter packets for valid CRC"));
  Serial.println(F("  s: Start listening"));
}

static void DumpPrompt()
{
  Serial.print(F("\n?: "));
}

static void Config(int cmd)
{
  int i;

  if (cmd == 'd')
  {
    activateConf();
    DumpConfig();
  }
  else if (cmd == 'f')
  {
    Serial.println(F("Filter for valid CRC: "));
    Serial.println(F(" 0: disable"));
    Serial.println(F(" 1: enable"));
    DumpPrompt();
    while (!Serial.available())
      ;
    i = Serial.read() - 0x30;
    if ((i < 0) || (i > 1))
    {
      Serial.println(F("\nIllegal selection."));
    }
    else
    {
      Serial.print(F("\nCRC check changed to "));
      Serial.println(i);
      checkCRC = i;
    }
  }
  else if (cmd == 'h')
  {
    DumpMenu();
  }
  else if (cmd == 's')
  {
    activateConf();
    bOperate = true;
    return;
  }
  else
  {
    Serial.println(F("Unknown command. Type 'h' for command list."));
  }

  DumpPrompt();
}

void setup(void)
{
  pinMode(LED_PIN_STATUS, OUTPUT);

  Serial.begin(SER_BAUDRATE);
  Serial.flush();

  hmPackets.SetUnixTimeStamp(0x623C8EA3);

  Serial.println(F("-- Hoymiles test --"));

  radio1.begin();

  // Disable shockburst for receiving and decode payload manually
  radio1.setAutoAck(false);
  radio1.setRetries(0, 0);

  // Configure nRF IRQ input
  pinMode(RF1_IRQ_PIN, INPUT);

  activateConf();
}

void loop(void)
{
  while (!packetBuffer.empty())
  {
    if (!bOperate)
    {
      packetBuffer.popBack();
      continue;
    }

    // One or more records present
    NRF24_packet_t *p = packetBuffer.getBack();

    // Shift payload data due to 9-bit packet control field
    for (int16_t j = sizeof(p->packet) - 1; j >= 0; j--)
    {
      if (j > 0)
        p->packet[j] = (byte)(p->packet[j] >> 7) | (byte)(p->packet[j - 1] << 1);
      else
        p->packet[j] = (byte)(p->packet[j] >> 7);
    }

    serialHdr.timestamp = p->timestamp;
    serialHdr.packetsLost = p->packetsLost;

    // Check CRC
    uint16_t crc = 0xFFFF;
    crc = crc16((uint8_t *)&serialHdr.address, sizeof(serialHdr.address), crc, 0, BYTES_TO_BITS(sizeof(serialHdr.address)));
    // Payload length
    uint8_t payloadLen = ((p->packet[0] & 0x01) << 5) | (p->packet[1] >> 3);
    // Add one byte and one bit for 9-bit packet control field
    crc = crc16((uint8_t *)&p->packet[0], sizeof(p->packet), crc, 7, BYTES_TO_BITS(payloadLen + 1) + 1);

    if (checkCRC)
    {
      // If CRC is invalid only show lost packets
      if (((crc >> 8) != p->packet[payloadLen + 2]) || ((crc & 0xFF) != p->packet[payloadLen + 3]))
      {
        if (p->packetsLost > 0)
        {
          Serial.print(F(" Lost: "));
          Serial.println(p->packetsLost);
        }
        packetBuffer.popBack();
        continue;
      }

      // Dump a decoded packet only once
      if (lastCRC == crc)
      {
        packetBuffer.popBack();
        continue;
      }
      lastCRC = crc;
    }

    // Don't dump mysterious ack packages
    if(payloadLen == 0)
    {
        packetBuffer.popBack();
        continue;
    }

    // Write timestamp, packets lost, address and payload length
    printf(" %09lu ", serialHdr.timestamp);
    dumpData((uint8_t *)&serialHdr.packetsLost, sizeof(serialHdr.packetsLost));
    dumpData((uint8_t *)&serialHdr.address, sizeof(serialHdr.address));

    // Trailing bit?!?
    dumpData(&p->packet[0], 2);

    // Payload length from PCF
    dumpData(&payloadLen, sizeof(payloadLen));

    // Packet control field - PID Packet identification
    uint8_t val = (p->packet[1] >> 1) & 0x03;
    Serial.print(val);
    Serial.print(F("  "));

    if (payloadLen > 9)
    {
      dumpData(&p->packet[2], 1);
      dumpData(&p->packet[3], 4);
      dumpData(&p->packet[7], 4);

      uint16_t remain = payloadLen - 2 - 1 - 4 - 4 + 4;

      if (remain < 32)
      {
        dumpData(&p->packet[11], remain);
        printf_P(PSTR("%04X "), crc);

        if (((crc >> 8) != p->packet[payloadLen + 2]) || ((crc & 0xFF) != p->packet[payloadLen + 3]))
          Serial.print(0);
        else
          Serial.print(1);
      }
      else
      {
        Serial.print(F("Ill remain "));
        Serial.print(remain);
      }
    }
    else
    {
      dumpData(&p->packet[2], payloadLen + 2);
      printf_P(PSTR("%04X "), crc);
    }

    if (p->packetsLost > 0)
    {
      Serial.print(F(" Lost: "));
      Serial.print(p->packetsLost);
    }
    Serial.println(F(""));

    // Remove record as we're done with it.
    packetBuffer.popBack();
  }

  // Configuration using terminal commands
  if (Serial.available())
  {
    int cmd = Serial.read();
    if (bOperate && (cmd == 'c'))
    {
      bOperate = false;
      DumpMenu();
      DumpPrompt();
    }
    else if (bOperate && ((cmd == 's') || (cmd == 'w')))
    {
      uint64_t dest = cmd == 's' ? WR1_RADIO_ID : WR2_RADIO_ID;

      int32_t size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, DTU_RADIO_ID >> 8, DTU_RADIO_ID >> 8, 0x07, 0x00);

      SendPacket(dest, (uint8_t *)&sendBuf, size);
    }
    else if (bOperate && ((cmd == 'd') || (cmd == 'e')))
    {
      uint64_t dest = cmd == 'd' ? WR1_RADIO_ID : WR2_RADIO_ID;

      int32_t size = hmPackets.GetTimePacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8);

      SendPacket(dest, (uint8_t *)&sendBuf, size);
    }
    else if (bOperate && ((cmd == 'f') || (cmd == 'r')))
    {
      uint64_t dest = cmd == 'f' ? WR1_RADIO_ID : WR2_RADIO_ID;

      int32_t size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0xFF);

      SendPacket(dest, (uint8_t *)&sendBuf, size);
    }
    else if (bOperate && ((cmd == 'g') || (cmd == 't')))
    {
      uint64_t dest = cmd == 'g' ? WR1_RADIO_ID : WR2_RADIO_ID;

      int32_t size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x81);

      SendPacket(dest, (uint8_t *)&sendBuf, size);
    }
    else if (bOperate && ((cmd == 'h') || (cmd == 'z')))
    {
      uint64_t dest = cmd == 'h' ? WR1_RADIO_ID : WR2_RADIO_ID;

      int32_t size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x82);

      SendPacket(dest, (uint8_t *)&sendBuf, size);
    }
    else if (bOperate && (cmd == 'q'))
    {
      Serial.println(F("\nRadio1:"));
      radio1.printPrettyDetails();
    }
    else if (!bOperate)
    {
      Serial.println((char)cmd);
      Config(cmd);
    }
  }

  // Status LED
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval)
  {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // if the LED is off turn it on and vice-versa:
    ledState = not(ledState);
    // set the LED with the ledState of the variable:
    digitalWrite(LED_PIN_STATUS, ledState);
  }

  // Second timer
  if (millis() >= tickMillis)
  {
    static uint8_t toggle = 0;
    static uint8_t tel = 0;
    tickMillis += 200;
    if (++tickSec >= 5)
    {
      hmPackets.UnixTimeStampTick();
      tickSec = 0;
    }

    if (bOperate)
    {
      int32_t size;
      uint64_t dest = toggle ? WR1_RADIO_ID : WR2_RADIO_ID;

      if (tel > 3)
        tel = 0;
      if (tel == 0)
        size = hmPackets.GetTimePacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8);
      else if (tel == 1)
        size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x81);
      else if (tel == 2)
        size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x80);
      else if (tel == 3)
        size = hmPackets.GetCmdPacket((uint8_t *)&sendBuf, dest >> 8, DTU_RADIO_ID >> 8, 0x15, 0x83);

      SendPacket(dest, (uint8_t *)&sendBuf, size);

      toggle = !toggle;
      if (!toggle)
        tel++;
    }
  }
}

#define DEBUG_SEND 0
static void SendPacket(uint64_t dest, uint8_t *buf, uint8_t len)
{
#if DEBUG_SEND  
  Serial.print(F("Send... CH"));
#endif  
  DISABLE_EINT;
  radio1.stopListening();

#ifdef CHANNEL_HOP
  static uint8_t hop = 0;
#if DEBUG_SEND    
  if (channels[hop] < 10)
    Serial.print(F("0"));
  Serial.print(channels[hop]);
#endif  
  radio1.setChannel(channels[hop++]);
  if (hop >= sizeof(channels) / sizeof(channels[0]))
    hop = 0;
#else
#if DEBUG_SEND  
  if (DEFAULT_SEND_CHANNEL < 10)
    Serial.print(F("0"));
  Serial.print(DEFAULT_SEND_CHANNEL);
#endif  
  radio1.setChannel(DEFAULT_SEND_CHANNEL);
#endif

  radio1.openWritingPipe(dest);
#if DEBUG_SEND    
  if (dest == WR1_RADIO_ID)
    Serial.print(F(" WR1 "));
  else
    Serial.print(F(" WR2 "));
#endif    

  radio1.setCRCLength(RF24_CRC_16);
  radio1.enableDynamicPayloads();
  radio1.setAutoAck(true);
  radio1.setRetries(3, 15);

#if DEBUG_SEND    
  uint32_t st = micros();
  bool res = 
#endif  
  radio1.write(buf, len);
#if DEBUG_SEND    
  Serial.print(res);
  Serial.print(F(" "));
  Serial.print(micros());
  Serial.print(F(" "));
  Serial.println(micros() - st);
#endif

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
