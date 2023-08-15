//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __RADIO_H__
#define __RADIO_H__

#include "../utils/dbg.h"
#include <RF24.h>
#include "../utils/crc.h"
#include "../config/config.h"
#include "SPI.h"

#define SPI_SPEED           1000000

#define RF_CHANNELS         5

#define TX_REQ_INFO         0x15
#define TX_REQ_DEVCONTROL   0x51
#define ALL_FRAMES          0x80
#define SINGLE_FRAME        0x81

const char* const rf24AmpPowerNames[] = {"MIN", "LOW", "HIGH", "MAX"};


//-----------------------------------------------------------------------------
// HM Radio class
//-----------------------------------------------------------------------------
template <uint8_t IRQ_PIN = DEF_IRQ_PIN, uint8_t CE_PIN = DEF_CE_PIN, uint8_t CS_PIN = DEF_CS_PIN, uint8_t AMP_PWR = RF24_PA_LOW, uint8_t SCLK_PIN = DEF_SCLK_PIN, uint8_t MOSI_PIN = DEF_MOSI_PIN, uint8_t MISO_PIN = DEF_MISO_PIN>
class HmRadio {
    public:
        HmRadio() : mNrf24(CE_PIN, CS_PIN, SPI_SPEED) {
            if(mSerialDebug) {
            DPRINT(DBG_VERBOSE, F("hmRadio.h : HmRadio():mNrf24(CE_PIN: "));
                DBGPRINT(String(CE_PIN));
                DBGPRINT(F(", CS_PIN: "));
                DBGPRINT(String(CS_PIN));
                DBGPRINT(F(", SPI_SPEED: "));
                DBGPRINT(String(SPI_SPEED));
                DBGPRINTLN(F(")"));
            }

            // Depending on the program, the module can work on 2403, 2423, 2440, 2461 or 2475MHz.
            // Channel List      2403, 2423, 2440, 2461, 2475MHz
            mRfChLst[0] = 03;
            mRfChLst[1] = 23;
            mRfChLst[2] = 40;
            mRfChLst[3] = 61;
            mRfChLst[4] = 75;

            // default channels
            mTxChIdx    = 2; // Start TX with 40
            mRxChIdx    = 0; // Start RX with 03

            mSendCnt        = 0;
            mRetransmits    = 0;

            mSerialDebug    = false;
            mIrqRcvd        = false;
        }
        ~HmRadio() {}

        void setup(uint8_t ampPwr = RF24_PA_LOW, uint8_t irq = IRQ_PIN, uint8_t ce = CE_PIN, uint8_t cs = CS_PIN, uint8_t sclk = SCLK_PIN, uint8_t mosi = MOSI_PIN, uint8_t miso = MISO_PIN) {
            DPRINTLN(DBG_VERBOSE, F("hmRadio.h:setup"));
            pinMode(irq, INPUT_PULLUP);

            uint32_t dtuSn = 0x87654321;
            uint32_t chipID = 0; // will be filled with last 3 bytes of MAC
            #ifdef ESP32
            uint64_t MAC = ESP.getEfuseMac();
            chipID = ((MAC >> 8) & 0xFF0000) | ((MAC >> 24) & 0xFF00) | ((MAC >> 40) & 0xFF);
            #else
            chipID = ESP.getChipId();
            #endif
            if(chipID) {
                dtuSn = 0x80000000; // the first digit is an 8 for DTU production year 2022, the rest is filled with the ESP chipID in decimal
                for(int i = 0; i < 7; i++) {
                    dtuSn |= (chipID % 10) << (i * 4);
                    chipID /= 10;
                }
            }
            // change the byte order of the DTU serial number and append the required 0x01 at the end
            DTU_RADIO_ID = ((uint64_t)(((dtuSn >> 24) & 0xFF) | ((dtuSn >> 8) & 0xFF00) | ((dtuSn << 8) & 0xFF0000) | ((dtuSn << 24) & 0xFF000000)) << 8) | 0x01;

            #ifdef ESP32
                #if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
                    mSpi = new SPIClass(HSPI);
                #else
                    mSpi = new SPIClass(VSPI);
                #endif
                mSpi->begin(sclk, miso, mosi, cs);
            #else
                //the old ESP82xx cannot freely place their SPI pins
                mSpi = new SPIClass();
                mSpi->begin();
            #endif
            mNrf24.begin(mSpi, ce, cs);
            mNrf24.setRetries(3, 15); // 3*250us + 250us and 15 loops -> 15ms

            mNrf24.setChannel(mRfChLst[mRxChIdx]);
            mNrf24.startListening();
            mNrf24.setDataRate(RF24_250KBPS);
            mNrf24.setAutoAck(true);
            mNrf24.enableDynamicPayloads();
            mNrf24.setCRCLength(RF24_CRC_16);
            mNrf24.setAddressWidth(5);
            mNrf24.openReadingPipe(1, reinterpret_cast<uint8_t*>(&DTU_RADIO_ID));

            // enable all receiving interrupts
            mNrf24.maskIRQ(false, false, false);

            DPRINT(DBG_INFO, F("RF24 Amp Pwr: RF24_PA_"));
            DPRINTLN(DBG_INFO, String(rf24AmpPowerNames[ampPwr]));
            mNrf24.setPALevel(ampPwr & 0x03);

            if(mNrf24.isChipConnected()) {
                DPRINTLN(DBG_INFO, F("Radio Config:"));
                mNrf24.printPrettyDetails();
            }
            else
                DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));
        }

        bool loop(void) {
            if (!mIrqRcvd)
                return false; // nothing to do
            mIrqRcvd = false;
            bool tx_ok, tx_fail, rx_ready;
            mNrf24.whatHappened(tx_ok, tx_fail, rx_ready);  // resets the IRQ pin to HIGH
            mNrf24.flush_tx();                              // empty TX FIFO

            // start listening
            mNrf24.setChannel(mRfChLst[mRxChIdx]);
            mNrf24.startListening();

            uint32_t startMicros = micros();
            uint32_t loopMillis = millis();
            while (millis()-loopMillis < 400) {
                while (micros()-startMicros < 5110) {  // listen (4088us or?) 5110us to each channel
                    if (mIrqRcvd) {
                        mIrqRcvd = false;
                        if (getReceived()) {        // everything received
                            return true;
                        }
                    }
                    yield();
                }
                // switch to next RX channel
                startMicros = micros();
                if(++mRxChIdx >= RF_CHANNELS)
                    mRxChIdx = 0;
                mNrf24.setChannel(mRfChLst[mRxChIdx]);
                yield();
            }
            // not finished but time is over
            return true;
        }

        void handleIntr(void) {
            mIrqRcvd = true;
        }

        bool isChipConnected(void) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:isChipConnected"));
            return mNrf24.isChipConnected();
        }
        void enableDebug() {
            mSerialDebug = true;
        }

        void sendControlPacket(uint64_t invId, uint8_t cmd, uint16_t *data, bool isRetransmit, bool isNoMI = true) {
            DPRINT(DBG_INFO, F("sendControlPacket cmd: 0x"));
            DBGHEXLN(cmd);
            initPacket(invId, TX_REQ_DEVCONTROL, SINGLE_FRAME);
            uint8_t cnt = 10;
            if (isNoMI) {
                mTxBuf[cnt++] = cmd; // cmd -> 0 on, 1 off, 2 restart, 11 active power, 12 reactive power, 13 power factor
                mTxBuf[cnt++] = 0x00;
                if(cmd >= ActivePowerContr && cmd <= PFSet) { // ActivePowerContr, ReactivePowerContr, PFSet
                    mTxBuf[cnt++] = ((data[0] * 10) >> 8) & 0xff; // power limit
                    mTxBuf[cnt++] = ((data[0] * 10)     ) & 0xff; // power limit
                    mTxBuf[cnt++] = ((data[1]     ) >> 8) & 0xff; // setting for persistens handlings
                    mTxBuf[cnt++] = ((data[1]     )     ) & 0xff; // setting for persistens handling
                }
            } else { //MI 2nd gen. specific
                switch (cmd) {
                    case TurnOn:
                        //mTxBuf[0] = 0x50;
                        mTxBuf[9] = 0x55;
                        mTxBuf[10] = 0xaa;
                        break;
                    case TurnOff:
                        mTxBuf[9] = 0xaa;
                        mTxBuf[10] = 0x55;
                        break;
                    case ActivePowerContr:
                        cnt++;
                        mTxBuf[9] = 0x5a;
                        mTxBuf[10] = 0x5a;
                        mTxBuf[11] = data[0]; // power limit
                        break;
                    default:
                        return;
                }
                cnt++;
            }
            sendPacket(invId, cnt, isRetransmit, isNoMI);
        }

        void prepareDevInformCmd(uint64_t invId, uint8_t cmd, uint32_t ts, uint16_t alarmMesId, bool isRetransmit, uint8_t reqfld=TX_REQ_INFO) { // might not be necessary to add additional arg.
            if(mSerialDebug) {
                DPRINT(DBG_DEBUG, F("prepareDevInformCmd 0x"));
                DPRINTLN(DBG_DEBUG,String(cmd, HEX));
            }
            initPacket(invId, reqfld, ALL_FRAMES);
            mTxBuf[10] = cmd; // cid
            mTxBuf[11] = 0x00;
            CP_U32_LittleEndian(&mTxBuf[12], ts);
            /*if (cmd == AlarmData ) { //cmd == RealTimeRunData_Debug ||
                mTxBuf[18] = (alarmMesId >> 8) & 0xff;
                mTxBuf[19] = (alarmMesId     ) & 0xff;
            }*/
            sendPacket(invId, 24, isRetransmit);
        }

        void sendCmdPacket(uint64_t invId, uint8_t mid, uint8_t pid, bool isRetransmit, bool appendCrc16=true) {
            initPacket(invId, mid, pid);
            sendPacket(invId, 10, isRetransmit, appendCrc16);
        }

        uint8_t getDataRate(void) {
            if(!mNrf24.isChipConnected())
                return 3; // unkown
            return mNrf24.getDataRate();
        }

        bool isPVariant(void) {
            return mNrf24.isPVariant();
        }

        std::queue<packet_t> mBufCtrl;

        uint32_t mSendCnt;
        uint32_t mRetransmits;

        bool mSerialDebug;

    private:
        bool getReceived(void) {
            bool tx_ok, tx_fail, rx_ready;
            mNrf24.whatHappened(tx_ok, tx_fail, rx_ready); // resets the IRQ pin to HIGH

            bool isLastPackage = false;
            while(mNrf24.available()) {
                uint8_t len;
                len = mNrf24.getDynamicPayloadSize(); // if payload size > 32, corrupt payload has been flushed
                if (len > 0) {
                    packet_t p;
                    p.ch = mRfChLst[mRxChIdx];
                    p.len = len;
                    mNrf24.read(p.packet, len);
                    if (p.packet[0] != 0x00) {
                    mBufCtrl.push(p);
                    if (p.packet[0] == (TX_REQ_INFO + ALL_FRAMES))  // response from get information command
                            isLastPackage = (p.packet[9] > ALL_FRAMES); // > ALL_FRAMES indicates last packet received
                    else if (p.packet[0] == ( 0x0f + ALL_FRAMES) )  // response from MI get information command
                            isLastPackage = (p.packet[9] > 0x10);       // > 0x10 indicates last packet received
                        else if ((p.packet[0] != 0x88) && (p.packet[0] != 0x92)) // ignore fragment number zero and MI status messages //#0 was p.packet[0] != 0x00 &&
                        isLastPackage = true;                       // response from dev control command
                    }
                }
                    yield();
                }
            return isLastPackage;
        }

        void initPacket(uint64_t invId, uint8_t mid, uint8_t pid) {
            if(mSerialDebug) {
                DPRINT(DBG_VERBOSE, F("initPacket, mid: "));
                DPRINT(DBG_VERBOSE, String(mid, HEX));
                DPRINT(DBG_VERBOSE,F(" pid: "));
                DPRINTLN(DBG_VERBOSE,String(pid, HEX));
            }
            memset(mTxBuf, 0, MAX_RF_PAYLOAD_SIZE);
            mTxBuf[0] = mid; // message id
            CP_U32_BigEndian(&mTxBuf[1], (invId  >> 8));
            CP_U32_BigEndian(&mTxBuf[5], (DTU_RADIO_ID >> 8));
            mTxBuf[9]  = pid;
        }

        void sendPacket(uint64_t invId, uint8_t len, bool isRetransmit, bool appendCrc16=true) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:sendPacket"));
            //DPRINTLN(DBG_VERBOSE, "sent packet: #" + String(mSendCnt));

            // append crc's
            if (appendCrc16 && (len > 10)) {
                // crc control data
                uint16_t crc = ah::crc16(&mTxBuf[10], len - 10);
                mTxBuf[len++] = (crc >> 8) & 0xff;
                mTxBuf[len++] = (crc     ) & 0xff;
            }
            // crc over all
            mTxBuf[len] = ah::crc8(mTxBuf, len);
            len++;

            // set TX and RX channels
            mTxChIdx = (mTxChIdx + 1) % RF_CHANNELS;
            mRxChIdx = (mTxChIdx + 2) % RF_CHANNELS;

            if(mSerialDebug) {
                DPRINT(DBG_INFO, F("TX "));
                DBGPRINT(String(len));
                DBGPRINT("B Ch");
                DBGPRINT(String(mRfChLst[mTxChIdx]));
                DBGPRINT(F(" | "));
                ah::dumpBuf(mTxBuf, len);
            }

            mNrf24.stopListening();
            mNrf24.setChannel(mRfChLst[mTxChIdx]);
            mNrf24.openWritingPipe(reinterpret_cast<uint8_t*>(&invId));
            mNrf24.startWrite(mTxBuf, len, false); // false = request ACK response

            if(isRetransmit)
                mRetransmits++;
            else
                mSendCnt++;
        }

        volatile bool mIrqRcvd;
        uint64_t DTU_RADIO_ID;

        uint8_t mRfChLst[RF_CHANNELS];
        uint8_t mTxChIdx;
        uint8_t mRxChIdx;

        SPIClass* mSpi;
        RF24 mNrf24;
        uint8_t mTxBuf[MAX_RF_PAYLOAD_SIZE];
};

#endif /*__RADIO_H__*/
