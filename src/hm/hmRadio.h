//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HM_RADIO_H__
#define __HM_RADIO_H__

#include <RF24.h>
#include "SPI.h"
#include "radio.h"

#define SPI_SPEED           1000000

#define RF_CHANNELS         5

const char* const rf24AmpPowerNames[] = {"MIN", "LOW", "HIGH", "MAX"};


//-----------------------------------------------------------------------------
// HM Radio class
//-----------------------------------------------------------------------------
template <uint8_t IRQ_PIN = DEF_NRF_IRQ_PIN, uint8_t CE_PIN = DEF_NRF_CE_PIN, uint8_t CS_PIN = DEF_NRF_CS_PIN, uint8_t AMP_PWR = RF24_PA_LOW, uint8_t SCLK_PIN = DEF_NRF_SCLK_PIN, uint8_t MOSI_PIN = DEF_NRF_MOSI_PIN, uint8_t MISO_PIN = DEF_NRF_MISO_PIN, uint32_t DTU_SN = 0x81001765>
class HmRadio : public Radio {
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
            mDtuSn = DTU_SN;

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

            mSerialDebug    = false;
            mIrqRcvd        = false;
        }
        ~HmRadio() {}

        void setup(uint8_t ampPwr = RF24_PA_LOW, uint8_t irq = IRQ_PIN, uint8_t ce = CE_PIN, uint8_t cs = CS_PIN, uint8_t sclk = SCLK_PIN, uint8_t mosi = MOSI_PIN, uint8_t miso = MISO_PIN) {
            DPRINTLN(DBG_VERBOSE, F("hmRadio.h:setup"));
            pinMode(irq, INPUT_PULLUP);

            generateDtuSn();
            DTU_RADIO_ID = ((uint64_t)(((mDtuSn >> 24) & 0xFF) | ((mDtuSn >> 8) & 0xFF00) | ((mDtuSn << 8) & 0xFF0000) | ((mDtuSn << 24) & 0xFF000000)) << 8) | 0x01;

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
            mNrf24.enableDynamicAck();
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
                DPRINT(DBG_INFO, F("DTU_SN: 0x"));
                DBGPRINTLN(String(mDtuSn, HEX));
            } else
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

        bool isChipConnected(void) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:isChipConnected"));
            return mNrf24.isChipConnected();
        }

        void sendControlPacket(Inverter<> *iv, uint8_t cmd, uint16_t *data, bool isRetransmit, bool isNoMI = true, bool is4chMI = false) {
            DPRINT(DBG_INFO, F("sendControlPacket cmd: 0x"));
            DBGHEXLN(cmd);
            initPacket(iv->radioId.u64, TX_REQ_DEVCONTROL, SINGLE_FRAME);
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
                        mTxBuf[9] = 0x5a;
                        mTxBuf[10] = 0x5a;
                        //Testing only! Original NRF24_DTUMIesp.ino code #L612-L613:
                        //UsrData[0]=0x5A;UsrData[1]=0x5A;UsrData[2]=100;//0x0a;// 10% limit
                        //UsrData[3]=((Limit*10) >> 8) & 0xFF;   UsrData[4]= (Limit*10)  & 0xFF;   //WR needs 1 dec= zB 100.1 W
                        if (is4chMI) {
                            mTxBuf[cnt++] = 100; //10% limit, seems to be necessary to send sth. at all, but for MI-1500 this has no effect
                            //works (if ever!) only for absulute power limits!
                            mTxBuf[cnt++] = ((data[0] * 10) >> 8) & 0xff; // power limit
                            mTxBuf[cnt++] = ((data[0] * 10)     ) & 0xff; // power limit
                        } else {
                            mTxBuf[cnt++] = data[0]*10; // power limit
                        }


                        break;
                    default:
                        return;
                }
                cnt++;
            }
            sendPacket(iv, cnt, isRetransmit, isNoMI);
        }

        uint8_t getDataRate(void) {
            if(!mNrf24.isChipConnected())
                return 3; // unknown
            return mNrf24.getDataRate();
        }

        bool isPVariant(void) {
            return mNrf24.isPVariant();
        }

        std::queue<packet_t> mBufCtrl;

    private:
        inline bool getReceived(void) {
            bool tx_ok, tx_fail, rx_ready;
            mNrf24.whatHappened(tx_ok, tx_fail, rx_ready); // resets the IRQ pin to HIGH

            bool isLastPackage = false;
            while(mNrf24.available()) {
                uint8_t len;
                len = mNrf24.getDynamicPayloadSize(); // if payload size > 32, corrupt payload has been flushed
                if (len > 0) {
                    packet_t p;
                    p.ch = mRfChLst[mRxChIdx];
                    p.len = (len > MAX_RF_PAYLOAD_SIZE) ? MAX_RF_PAYLOAD_SIZE : len;
                    p.rssi = mNrf24.testRPD() ? -64 : -75;
                    mNrf24.read(p.packet, p.len);
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

        void sendPacket(Inverter<> *iv, uint8_t len, bool isRetransmit, bool appendCrc16=true) {
            updateCrcs(&len, appendCrc16);

            // set TX and RX channels
            mTxChIdx = (mTxChIdx + 1) % RF_CHANNELS;
            mRxChIdx = (mTxChIdx + 2) % RF_CHANNELS;

            if(mSerialDebug) {
                DPRINT(DBG_INFO, F("TX "));
                DBGPRINT(String(len));
                DBGPRINT(" CH");
                DBGPRINT(String(mRfChLst[mTxChIdx]));
                DBGPRINT(F(" | "));
                ah::dumpBuf(mTxBuf, len);
            }

            mNrf24.stopListening();
            mNrf24.setChannel(mRfChLst[mTxChIdx]);
            mNrf24.openWritingPipe(reinterpret_cast<uint8_t*>(&iv->radioId.u64));
            mNrf24.startWrite(mTxBuf, len, false); // false = request ACK response

            if(isRetransmit)
                iv->radioStatistics.retransmits++;
            else
                iv->radioStatistics.txCnt++;
        }

        uint64_t getIvId(Inverter<> *iv) {
            return iv->radioId.u64;
        }

        uint64_t DTU_RADIO_ID;
        uint8_t mRfChLst[RF_CHANNELS];
        uint8_t mTxChIdx;
        uint8_t mRxChIdx;

        SPIClass* mSpi;
        RF24 mNrf24;
};

#endif /*__HM_RADIO_H__*/
