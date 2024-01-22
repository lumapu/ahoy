//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HM_RADIO_H__
#define __HM_RADIO_H__

#include <RF24.h>
#include "SPI.h"
#include "radio.h"
#include "../config/config.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(SPI_HAL)
#include "nrfHal.h"
#endif

#define SPI_SPEED           1000000

#define RF_CHANNELS         5

const char* const rf24AmpPowerNames[] = {"MIN", "LOW", "HIGH", "MAX"};

#define TX_REQ_DREDCONTROL  0x50
#define DRED_A5 0xa5
#define DRED_5A 0x5a
#define DRED_AA 0xaa
#define DRED_55 0x55

//-----------------------------------------------------------------------------
// HM Radio class
//-----------------------------------------------------------------------------
template <uint8_t IRQ_PIN = DEF_NRF_IRQ_PIN, uint8_t CE_PIN = DEF_NRF_CE_PIN, uint8_t CS_PIN = DEF_NRF_CS_PIN, uint8_t AMP_PWR = RF24_PA_LOW, uint8_t SCLK_PIN = DEF_NRF_SCLK_PIN, uint8_t MOSI_PIN = DEF_NRF_MOSI_PIN, uint8_t MISO_PIN = DEF_NRF_MISO_PIN, uint32_t DTU_SN = 0x81001765>
class HmRadio : public Radio {
    public:
        HmRadio() {
            mDtuSn   = DTU_SN;
            mIrqRcvd = false;
            #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(SPI_HAL)
            //mNrf24.reset(new RF24());
            #else
            mNrf24.reset(new RF24(CE_PIN, CS_PIN, SPI_SPEED));
            #endif
        }
        ~HmRadio() {}

        void setup(bool *serialDebug, bool *privacyMode, bool *printWholeTrace, uint8_t irq = IRQ_PIN, uint8_t ce = CE_PIN, uint8_t cs = CS_PIN, uint8_t sclk = SCLK_PIN, uint8_t mosi = MOSI_PIN, uint8_t miso = MISO_PIN) {
            DPRINTLN(DBG_VERBOSE, F("hmRadio.h:setup"));

            pinMode(irq, INPUT_PULLUP);

            mSerialDebug     = serialDebug;
            mPrivacyMode     = privacyMode;
            mPrintWholeTrace = printWholeTrace;

            generateDtuSn();
            DTU_RADIO_ID = ((uint64_t)(((mDtuSn >> 24) & 0xFF) | ((mDtuSn >> 8) & 0xFF00) | ((mDtuSn << 8) & 0xFF0000) | ((mDtuSn << 24) & 0xFF000000)) << 8) | 0x01;

            #ifdef ESP32
                #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(SPI_HAL)
                    mNrfHal.init(mosi, miso, sclk, cs, ce, SPI_SPEED);
                    mNrf24.reset(new RF24(&mNrfHal));
                #else
                    #if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
                        mSpi.reset(new SPIClass(HSPI));
                    #else
                        mSpi.reset(new SPIClass(VSPI));
                    #endif
                    mSpi->begin(sclk, miso, mosi, cs);
                #endif
            #else
                //the old ESP82xx cannot freely place their SPI pins
                mSpi.reset(new SPIClass());
                mSpi->begin();
            #endif

            #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(SPI_HAL)
                mNrf24->begin();
            #else
                mNrf24->begin(mSpi.get(), ce, cs);
            #endif
            mNrf24->setRetries(3, 15); // wait 3*250 = 750us, 16 * 250us -> 4000us = 4ms

            mNrf24->setDataRate(RF24_250KBPS);
            //mNrf24->setAutoAck(true); // enabled by default
            //mNrf24->enableDynamicAck();
            mNrf24->enableDynamicPayloads();
            mNrf24->setCRCLength(RF24_CRC_16);
            mNrf24->setAddressWidth(5);
            mNrf24->openReadingPipe(1, reinterpret_cast<uint8_t*>(&DTU_RADIO_ID));
            mNrf24->maskIRQ(false, false, false); // enable all receiving interrupts
            mNrf24->setPALevel(1); // low is default

            if(mNrf24->isChipConnected()) {
                DPRINTLN(DBG_INFO, F("Radio Config:"));
                mNrf24->printPrettyDetails();
                DPRINT(DBG_INFO, F("DTU_SN: "));
                DBGPRINTLN(String(mDtuSn, HEX));
            } else
                DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));
        }

        // returns true if communication is active
        bool loop(void) {
            if (!mIrqRcvd && !mNRFisInRX)
                return false; // first quick check => nothing to do at all here

            if(NULL == mLastIv) // prevent reading on NULL object!
                return false;

            if(!mIrqRcvd) {     // no news from nRF, check timers
                if ((millis() - mTimeslotStart) < innerLoopTimeout)
                    return true; // nothing to do, still waiting

                if (mRadioWaitTime.isTimeout()) { // timeout reached!
                    mNRFisInRX = false;
                    return false;
                }

                // otherwise switch to next RX channel
                mTimeslotStart = millis();
                if(!mNRFloopChannels && ((mTimeslotStart - mLastIrqTime) > (DURATION_TXFRAME+DURATION_ONEFRAME)))
                    mNRFloopChannels = true;

                rxPendular = !rxPendular;
                //innerLoopTimeout = (rxPendular ? 1 : 2)*DURATION_LISTEN_MIN;
                innerLoopTimeout = DURATION_LISTEN_MIN;

                if(mNRFloopChannels)
                    tempRxChIdx = (tempRxChIdx + 4) % RF_CHANNELS;
                else
                    tempRxChIdx = (mRxChIdx + rxPendular*4) % RF_CHANNELS;

                mNrf24->setChannel(mRfChLst[tempRxChIdx]);
                isRxInit = false;

                return true; // communicating, but changed RX channel
            } else {
                // here we got news from the nRF
                mIrqRcvd     = false;
                mNrf24->whatHappened(tx_ok, tx_fail, rx_ready); // resets the IRQ pin to HIGH
                mLastIrqTime = millis();

                if(tx_ok || tx_fail) { // tx related interrupt, basically we should start listening
                    mNrf24->flush_tx();                         // empty TX FIFO
                    mTxSetupTime = millis() - mMillis;

                    if(mNRFisInRX) {
                        DPRINTLN(DBG_WARN, F("unexpected tx irq!"));
                        return false;
                    }

                    mNRFisInRX = true;
                    if(tx_ok)
                        mLastIv->mAckCount++;

                    mRxChIdx = (mTxChIdx + 2) % RF_CHANNELS;
                    mNrf24->setChannel(mRfChLst[mRxChIdx]);
                    mNrf24->startListening();
                    mTimeslotStart = millis();
                    tempRxChIdx = mRxChIdx;
                    rxPendular  = false;
                    mNRFloopChannels = (mLastIv->ivGen == IV_MI);

                    innerLoopTimeout = DURATION_TXFRAME;
                }

                if(rx_ready) {
                    if (getReceived()) { // check what we got, returns true for last package
                        mNRFisInRX = false;
                        mRadioWaitTime.startTimeMonitor(DURATION_PAUSE_LASTFR); // let the inverter first end his transmissions
                        mNrf24->stopListening();
                    } else {
                        innerLoopTimeout = DURATION_LISTEN_MIN;
                        mTimeslotStart = millis();
                        if (!mNRFloopChannels) {
                            //rxPendular = true; // stay longer on the next rx channel
                            if (isRxInit) {
                                isRxInit = false;
                                tempRxChIdx = (mRxChIdx + 4) % RF_CHANNELS;
                                mNrf24->setChannel(mRfChLst[tempRxChIdx]);
                            } else
                                mRxChIdx = tempRxChIdx;
                        }
                    }
                    return mNRFisInRX;
                } /*else if(tx_fail) {
                    mNRFisInRX = false;
                    return false;
                }*/
            }

            return false;
        }

        bool isChipConnected(void) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:isChipConnected"));
            return mNrf24->isChipConnected();
        }

        void sendControlPacket(Inverter<> *iv, uint8_t cmd, uint16_t *data, bool isRetransmit) {
            DPRINT_IVID(DBG_INFO, iv->id);
            DBGPRINT(F("sendControlPacket cmd: "));
            DBGHEXLN(cmd);
            initPacket(iv->radioId.u64, TX_REQ_DEVCONTROL, SINGLE_FRAME);
            uint8_t cnt = 10;
            if (IV_MI != iv->ivGen) {
                mTxBuf[cnt++] = cmd; // cmd -> 0 on, 1 off, 2 restart, 11 active power, 12 reactive power, 13 power factor
                mTxBuf[cnt++] = 0x00;
                if(cmd >= ActivePowerContr && cmd <= PFSet) { // ActivePowerContr, ReactivePowerContr, PFSet
                    mTxBuf[cnt++] = (data[0] >> 8) & 0xff; // power limit, multiplied by 10 (because of fraction)
                    mTxBuf[cnt++] = (data[0]     ) & 0xff; // power limit
                    mTxBuf[cnt++] = (data[1] >> 8) & 0xff; // setting for persistens handlings
                    mTxBuf[cnt++] = (data[1]     ) & 0xff; // setting for persistens handling
                }
            } else { //MI 2nd gen. specific
                uint16_t powerMax = ((iv->powerLimit[1] == RelativNonPersistent) ? 0 : iv->getMaxPower());
                switch (cmd) {
                    case Restart:
                    case TurnOn:
                        mTxBuf[9]  = DRED_55;
                        mTxBuf[10] = DRED_AA;
                        break;
                    case TurnOff:
                        mTxBuf[9]  = DRED_AA;
                        mTxBuf[10] = DRED_55;
                        break;
                    case ActivePowerContr:
                        if (data[1]<256) { // non persistent
                            mTxBuf[9]  = DRED_5A;
                            mTxBuf[10] = DRED_5A;
                            //Testing only! Original NRF24_DTUMIesp.ino code #L612-L613:
                            //UsrData[0]=0x5A;UsrData[1]=0x5A;UsrData[2]=100;//0x0a;// 10% limit
                            //UsrData[3]=((Limit*10) >> 8) & 0xFF;   UsrData[4]= (Limit*10)  & 0xFF;   //WR needs 1 dec= zB 100.1 W
                            if (!data[1]) {   //     AbsolutNonPersistent
                                mTxBuf[++cnt] = 100; //10% limit, seems to be necessary to send sth. at all, but for MI-1500 this has no effect
                                //works (if ever!) only for absulute power limits!
                                mTxBuf[++cnt] = ((data[0] * 10) >> 8) & 0xff; // power limit in W
                                mTxBuf[++cnt] = ((data[0] * 10)     ) & 0xff; // power limit in W
                            } else if (powerMax) {       //relative, but 4ch-MI (if ever) only accepts absolute values
                                mTxBuf[++cnt] = data[0]; // simple power limit in %, might be necessary to multiply by 10?
                                mTxBuf[++cnt] = ((data[0] * 10 * powerMax) >> 8) & 0xff; // power limit
                                mTxBuf[++cnt] = ((data[0] * 10 * powerMax)     ) & 0xff; // power limit
                            } else {   // might work for 1/2ch MI (if ever)
                                mTxBuf[++cnt] = data[0]; // simple power limit in %, might be necessary to multiply by 10?
                            }
                        } else {       // persistent power limit needs to be translated in DRED command (?)
                            /* DRED instruction
                            Order	 Function
                            0x55AA	 Boot without DRM restrictions
                            0xA5A5	 DRM0 shutdown
                            0x5A5A	 DRM5 power limit 0%
                            0xAA55	 DRM6 power limit 50%
                            0x5A55	 DRM8 unlimited power operation
                            */
                            mTxBuf[0] = TX_REQ_DREDCONTROL;

                            if (data[1] == 256UL) {   //     AbsolutPersistent
                                if (data[0] == 0 && !powerMax) {
                                    mTxBuf[9]  = DRED_A5;
                                    mTxBuf[10] = DRED_A5;
                                } else if (data[0] == 0 || !powerMax || data[0] < powerMax/4 ) {
                                    mTxBuf[9]  = DRED_5A;
                                    mTxBuf[10] = DRED_5A;
                                } else if (data[0] <=  powerMax/4*3) {
                                    mTxBuf[9]  = DRED_AA;
                                    mTxBuf[10] = DRED_55;
                                } else if (data[0] <=  powerMax) {
                                    mTxBuf[9]  = DRED_5A;
                                    mTxBuf[10] = DRED_55;
                                } else if (data[0] > powerMax*2) {
                                    mTxBuf[9]  = DRED_55;
                                    mTxBuf[10] = DRED_AA;
                                }
                            }
                        }
                        break;
                    default:
                        return;
                }
                cnt++;
            }

            sendPacket(iv, cnt, isRetransmit, (IV_MI != iv->ivGen));
        }

        uint8_t getDataRate(void) {
            if(!mNrf24->isChipConnected())
                return 3; // unknown
            return mNrf24->getDataRate();
        }

        bool isPVariant(void) {
            return mNrf24->isPVariant();
        }

        uint8_t getARC(void) {
            return mNrf24->getARC();
        }

        uint8_t getPLOS(void) {
            return mNrf24->getPLOS();
        }

    private:
        inline bool getReceived(void) {
            bool isLastPackage = false;
            rx_ready = false; // reset for ACK case

            while(mNrf24->available()) {
                uint8_t len = mNrf24->getDynamicPayloadSize(); // payload size > 32 -> corrupt payload

                if (len > 0) {
                    packet_t p;
                    p.ch   = mRfChLst[tempRxChIdx];
                    p.len  = (len > MAX_RF_PAYLOAD_SIZE) ? MAX_RF_PAYLOAD_SIZE : len;
                    p.rssi = mNrf24->testRPD() ? -64 : -75;
                    p.millis = millis() - mMillis;
                    p.arc  = mNrf24->getARC();
                    p.plos = mNrf24->getPLOS();
                    mNrf24->read(p.packet, p.len);

                    if (p.packet[0] != 0x00) {
                        if(!checkIvSerial(p.packet, mLastIv)) {
                            DPRINT(DBG_WARN, "RX other inverter ");
                            if(!*mPrivacyMode)
                                ah::dumpBuf(p.packet, p.len);
                        } else {
                            mLastIv->mGotFragment = true;
                            mBufCtrl.push(p);

                            if (p.packet[0] == (TX_REQ_INFO + ALL_FRAMES))  // response from get information command
                                isLastPackage = (p.packet[9] > ALL_FRAMES); // > ALL_FRAMES indicates last packet received

                            if(IV_MI == mLastIv->ivGen) {
                                if (p.packet[0] == (0x0f + ALL_FRAMES))                  // response from MI get information command
                                    isLastPackage = (p.packet[9] > 0x10);                // > 0x10 indicates last packet received
                                else if ((p.packet[0] != 0x88) && (p.packet[0] != 0x92)) // ignore MI status messages //#0 was p.packet[0] != 0x00 &&
                                    isLastPackage = true;                                // response from dev control command
                            }
                            rx_ready = true; //reset in case we first read messages from other inverter or ACK zero payloads
                        }
                    }
                }
                yield();
            }
            if(isLastPackage)
                mLastIv->mGotLastMsg = true;
            return isLastPackage;
        }

        void sendPacket(Inverter<> *iv, uint8_t len, bool isRetransmit, bool appendCrc16=true) {
            mNrf24->setPALevel(iv->config->powerLevel & 0x03);
            updateCrcs(&len, appendCrc16);

            // set TX and RX channels
            mTxChIdx = iv->heuristics.txRfChId;

            if(*mSerialDebug) {
                if(!isRetransmit) {
                    DPRINT(DBG_INFO, "last tx setup: ");
                    DBGPRINT(String(mTxSetupTime));
                    DBGPRINTLN("ms");
                }

                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINT(F("TX "));
                DBGPRINT(String(len));
                DBGPRINT(" CH");
                DBGPRINT(String(mRfChLst[mTxChIdx]));
                DBGPRINT(F(" | "));
                if(*mPrintWholeTrace) {
                    if(*mPrivacyMode)
                        ah::dumpBuf(mTxBuf, len, 1, 4);
                    else
                        ah::dumpBuf(mTxBuf, len);
                } else {
                    DHEX(mTxBuf[0]);
                    DBGPRINT(F(" "));
                    DHEX(mTxBuf[10]);
                    DBGPRINT(F(" "));
                    DBGHEXLN(mTxBuf[9]);
                }
            }

            mNrf24->stopListening();
            mNrf24->setChannel(mRfChLst[mTxChIdx]);
            mNrf24->openWritingPipe(reinterpret_cast<uint8_t*>(&iv->radioId.u64));
            mNrf24->startWrite(mTxBuf, len, false); // false = request ACK response
            mMillis = millis();

            mLastIv = iv;
            iv->mDtuTxCnt++;
            mNRFisInRX = false;
        }

        uint64_t getIvId(Inverter<> *iv) {
            return iv->radioId.u64;
        }

        uint8_t getIvGen(Inverter<> *iv) {
            return iv->ivGen;
        }

        inline bool checkIvSerial(uint8_t buf[], Inverter<> *iv) {
            for(uint8_t i = 1; i < 5; i++) {
                if(buf[i] != iv->radioId.b[i])
                    return false;
            }
            return true;
        }

        uint64_t DTU_RADIO_ID;
        uint8_t mRfChLst[RF_CHANNELS] = {03, 23, 40, 61, 75}; // channel List:2403, 2423, 2440, 2461, 2475MHz
        uint8_t mTxChIdx = 0;
        uint8_t mRxChIdx = 0;
        uint8_t tempRxChIdx = mRxChIdx;
        bool    mGotLastMsg = false;
        uint32_t mMillis;
        bool tx_ok, tx_fail, rx_ready = false;
        unsigned long mTimeslotStart = 0;
        unsigned long mLastIrqTime = 0;
        bool mNRFloopChannels = false;
        bool mNRFisInRX = false;
        bool isRxInit = true;
        bool rxPendular = false;
        uint32_t innerLoopTimeout = DURATION_LISTEN_MIN;
        uint8_t mTxSetupTime = 0;

        std::unique_ptr<SPIClass> mSpi;
        std::unique_ptr<RF24> mNrf24;
        #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(SPI_HAL)
        nrfHal mNrfHal;
        #endif
        Inverter<> *mLastIv = NULL;
};

#endif /*__HM_RADIO_H__*/
