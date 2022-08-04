//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __RADIO_H__
#define __RADIO_H__

#include "dbg.h"
#include <RF24.h>
#include <RF24_config.h>
#include "crc.h"

//#define CHANNEL_HOP // switch between channels or use static channel to send

#define DEFAULT_RECV_CHANNEL    3
#define SPI_SPEED               1000000

#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)
#define DUMMY_RADIO_ID          ((uint64_t)0xDEADBEEF01ULL)

#define RF_CHANNELS             5
#define RF_LOOP_CNT             300


const char* const rf24AmpPower[] = {"MIN", "LOW", "HIGH", "MAX"};


//-----------------------------------------------------------------------------
// MACROS
//-----------------------------------------------------------------------------
#define CP_U32_LittleEndian(buf, v) ({ \
    uint8_t *b = buf; \
    b[0] = ((v >> 24) & 0xff); \
    b[1] = ((v >> 16) & 0xff); \
    b[2] = ((v >>  8) & 0xff); \
    b[3] = ((v      ) & 0xff); \
})

#define CP_U32_BigEndian(buf, v) ({ \
    uint8_t *b = buf; \
    b[3] = ((v >> 24) & 0xff); \
    b[2] = ((v >> 16) & 0xff); \
    b[1] = ((v >>  8) & 0xff); \
    b[0] = ((v      ) & 0xff); \
})

#define BIT_CNT(x)  ((x)<<3)


//-----------------------------------------------------------------------------
// HM Radio class
//-----------------------------------------------------------------------------
template <uint8_t CE_PIN, uint8_t CS_PIN, uint8_t IRQ_PIN, class BUFFER, uint64_t DTU_ID=DTU_RADIO_ID>
class HmRadio {
    public:
        HmRadio() : mNrf24(CE_PIN, CS_PIN, SPI_SPEED) {
            DPRINTLN(DBG_VERBOSE, F("hmRadio.h : HmRadio():mNrf24(CE_PIN: ") + String(CE_PIN) + F(", CS_PIN: ") + String(CS_PIN) + F(", SPI_SPEED: ") + String(SPI_SPEED) + ")");

            // Depending on the program, the module can work on 2403, 2423, 2440, 2461 or 2475MHz.
            // Channel List      2403, 2423, 2440, 2461, 2475MHz
            mRfChLst[0] = 03;
            mRfChLst[1] = 23;
            mRfChLst[2] = 40;
            mRfChLst[3] = 61;
            mRfChLst[4] = 75;

            mTxChIdx    = 2; // Start TX with 40
            mRxChIdx    = 0; // Start RX with 03
            mRxLoopCnt  = RF_LOOP_CNT;

            pinCs  = CS_PIN;
            pinCe  = CE_PIN;
            pinIrq = IRQ_PIN;

            AmplifierPower = 1;
            mSendCnt       = 0;

            mSerialDebug = false;
            mIrqRcvd     = false;
        }
        ~HmRadio() {}

        void setup(BUFFER *ctrl) {
            DPRINTLN(DBG_VERBOSE, F("hmRadio.h:setup"));
            pinMode(pinIrq, INPUT_PULLUP);

            mBufCtrl = ctrl;

            mNrf24.begin(pinCe, pinCs);
            mNrf24.setRetries(0, 0);

            mNrf24.setChannel(DEFAULT_RECV_CHANNEL);
            mNrf24.setDataRate(RF24_250KBPS);
            mNrf24.setCRCLength(RF24_CRC_16);
            mNrf24.setAutoAck(false);
            mNrf24.setPayloadSize(MAX_RF_PAYLOAD_SIZE);
            mNrf24.setAddressWidth(5);
            mNrf24.openReadingPipe(1, DTU_RADIO_ID);
            mNrf24.enableDynamicPayloads();

            // enable only receiving interrupts
            mNrf24.maskIRQ(true, true, false);

            DPRINTLN(DBG_INFO, F("RF24 Amp Pwr: RF24_PA_") + String(rf24AmpPower[AmplifierPower]));
            mNrf24.setPALevel(AmplifierPower & 0x03);
            mNrf24.startListening();

            DPRINTLN(DBG_INFO, F("Radio Config:"));
            mNrf24.printPrettyDetails();

            mTxCh = setDefaultChannels();

            if(!mNrf24.isChipConnected()) {
                DPRINTLN(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));
            }
        }

        void loop(void) {
            DISABLE_IRQ;
            if(mIrqRcvd) {
                mIrqRcvd = false;
                bool tx_ok, tx_fail, rx_ready;
                mNrf24.whatHappened(tx_ok, tx_fail, rx_ready); // resets the IRQ pin to HIGH
                RESTORE_IRQ;
                uint8_t pipe, len;
                packet_t *p;
                while(mNrf24.available(&pipe)) {
                    if(!mBufCtrl->full()) {
                        p = mBufCtrl->getFront();
                        p->rxCh = mRfChLst[mRxChIdx];
                        len = mNrf24.getPayloadSize();
                        if(len > MAX_RF_PAYLOAD_SIZE)
                            len = MAX_RF_PAYLOAD_SIZE;

                        mNrf24.read(p->packet, len);
                        mBufCtrl->pushFront(p);
                    }
                    else {
                        mNrf24.flush_rx(); // drop the packet
                        break;
                    }
                    yield();
                }
            }
            else
                RESTORE_IRQ;
        }

        void handleIntr(void) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:handleIntr"));
            mIrqRcvd = true;
        }

        uint8_t setDefaultChannels(void) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:setDefaultChannels"));
            mTxChIdx    = 2; // Start TX with 40
            mRxChIdx    = 0; // Start RX with 03            
            return mRfChLst[mTxChIdx];
        }

        void sendTimePacket(uint64_t invId, uint32_t ts) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:sendTimePacket"));
            sendCmdPacket(invId, 0x15, 0x80, false);
            mTxBuf[10] = 0x0b; // cid
            mTxBuf[11] = 0x00;
            CP_U32_LittleEndian(&mTxBuf[12], ts);
            mTxBuf[19] = 0x05;

            uint16_t crc = crc16(&mTxBuf[10], 14);
            mTxBuf[24] = (crc >> 8) & 0xff;
            mTxBuf[25] = (crc     ) & 0xff;
            mTxBuf[26] = crc8(mTxBuf, 26);

            sendPacket(invId, mTxBuf, 27, true);
        }

        void sendCmdPacket(uint64_t invId, uint8_t mid, uint8_t pid, bool calcCrc = true) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:sendCmdPacket"));
            memset(mTxBuf, 0, MAX_RF_PAYLOAD_SIZE);
            mTxBuf[0] = mid; // message id
            CP_U32_BigEndian(&mTxBuf[1], (invId  >> 8));
            CP_U32_BigEndian(&mTxBuf[5], (DTU_ID >> 8));
            mTxBuf[9]  = pid;
            if(calcCrc) {
                mTxBuf[10] = crc8(mTxBuf, 10);
                sendPacket(invId, mTxBuf, 11, false);
            }
        }

        bool checkPaketCrc(uint8_t buf[], uint8_t *len, uint8_t rxCh) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:checkPaketCrc"));
            *len = (buf[0] >> 2);
            if(*len > (MAX_RF_PAYLOAD_SIZE - 2))
                *len = MAX_RF_PAYLOAD_SIZE - 2;
            for(uint8_t i = 1; i < (*len + 1); i++) {
                buf[i-1] = (buf[i] << 1) | (buf[i+1] >> 7);
            }

            uint8_t crc = crc8(buf, *len-1);
            bool valid  = (crc == buf[*len-1]);

            return valid;
        }

        bool switchRxCh(uint16_t addLoop = 0) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:switchRxCh"));
            //DPRINTLN(DBG_VERBOSE, F("R"));

            mRxLoopCnt += addLoop;
            if(mRxLoopCnt != 0) {
                mRxLoopCnt--;
                DISABLE_IRQ;
                mNrf24.stopListening();
                mNrf24.setChannel(getRxNxtChannel());
                mNrf24.startListening();
                RESTORE_IRQ;
            }
            return (0 == mRxLoopCnt); // receive finished
        }

        void dumpBuf(const char *info, uint8_t buf[], uint8_t len) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:dumpBuf"));
            if(NULL != info)
                DBGPRINT(String(info));
            for(uint8_t i = 0; i < len; i++) {
                DHEX(buf[i]);
                DBGPRINT(" ");
            }
            DBGPRINTLN("");
        }

        bool isChipConnected(void) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:isChipConnected"));
            return mNrf24.isChipConnected();
        }

        uint8_t pinCs;
        uint8_t pinCe;
        uint8_t pinIrq;

        uint8_t AmplifierPower;
        uint32_t mSendCnt;

        bool mSerialDebug;

    private:
        void sendPacket(uint64_t invId, uint8_t buf[], uint8_t len, bool clear=false) {
            //DPRINTLN(DBG_VERBOSE, F("hmRadio.h:sendPacket"));
            //DPRINTLN(DBG_VERBOSE, "sent packet: #" + String(mSendCnt));
            //dumpBuf("SEN ", buf, len);
            if(mSerialDebug) {
                DPRINT(DBG_INFO, "TX " + String(len) + "B Ch" + String(mRfChLst[mTxChIdx]) + " | ");
                dumpBuf(NULL, buf, len);
            }

            DISABLE_IRQ;
            mNrf24.stopListening();

            if(clear)
                mRxLoopCnt = RF_LOOP_CNT;

            mNrf24.setChannel(mRfChLst[mTxChIdx]);
            mTxCh = getTxNxtChannel(); // switch channel for next packet
            mNrf24.openWritingPipe(invId); // TODO: deprecated
            mNrf24.setCRCLength(RF24_CRC_16);
            mNrf24.enableDynamicPayloads();
            mNrf24.setAutoAck(true);
            mNrf24.setRetries(3, 15); // 3*250us and 15 loops -> 11.25ms
            mNrf24.write(buf, len);

            // Try to avoid zero payload acks (has no effect)
            mNrf24.openWritingPipe(DUMMY_RADIO_ID); // TODO: why dummy radio id?, deprecated
            mRxChIdx = 0;
            mNrf24.setChannel(mRfChLst[mRxChIdx]);
            mNrf24.setAutoAck(false);
            mNrf24.setRetries(0, 0);
            mNrf24.disableDynamicPayloads();
            mNrf24.setCRCLength(RF24_CRC_DISABLED);
            mNrf24.startListening();

            RESTORE_IRQ;
            mSendCnt++;
        }

        uint8_t getTxNxtChannel(void) {

            if(++mTxChIdx >= RF_CHANNELS)
                mTxChIdx = 0;
            return mRfChLst[mTxChIdx];
        }

        uint8_t getRxNxtChannel(void) {

            if(++mRxChIdx >= RF_CHANNELS)
                mRxChIdx = 0;
            return mRfChLst[mRxChIdx];
        }

        uint8_t mTxCh;
        uint8_t mTxChIdx;

        uint8_t mRfChLst[RF_CHANNELS];
        
        uint8_t mRxChIdx;
        uint16_t mRxLoopCnt;

        RF24 mNrf24;
        BUFFER *mBufCtrl;
        uint8_t mTxBuf[MAX_RF_PAYLOAD_SIZE];

        volatile bool mIrqRcvd;
};

#endif /*__RADIO_H__*/
