#ifndef __RADIO_H__
#define __RADIO_H__

#include <RF24.h>
#include <RF24_config.h>
#include "crc.h"

//#define CHANNEL_HOP // switch between channels or use static channel to send

#define DEFAULT_RECV_CHANNEL    3
#define SPI_SPEED               1000000

#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)
#define DUMMY_RADIO_ID          ((uint64_t)0xDEADBEEF01ULL)

#define RX_LOOP_CNT             400

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
            mTxChLst[0] = 40;
            //mTxChIdx = 1;

            mRxChLst[0] = 3;
            mRxChLst[1] = 23;
            mRxChLst[2] = 61;
            mRxChLst[3] = 75;
            mRxChIdx    = 0;
            mRxLoopCnt  = RX_LOOP_CNT;

            pinCs  = CS_PIN;
            pinCe  = CE_PIN;
            pinIrq = IRQ_PIN;

            AmplifierPower = 1;
            mSendCnt       = 0;

            mSerialDebug = false;
        }
        ~HmRadio() {}

        void setup(BUFFER *ctrl) {
            pinMode(pinIrq, INPUT_PULLUP);

            mBufCtrl = ctrl;

            mNrf24.begin(pinCe, pinCs);
            mNrf24.setRetries(0, 0);

            mNrf24.setChannel(DEFAULT_RECV_CHANNEL);
            mNrf24.setDataRate(RF24_250KBPS);
            mNrf24.disableCRC();
            mNrf24.setAutoAck(false);
            mNrf24.setPayloadSize(MAX_RF_PAYLOAD_SIZE);
            mNrf24.setAddressWidth(5);
            mNrf24.openReadingPipe(1, DTU_RADIO_ID);

            // enable only receiving interrupts
            mNrf24.maskIRQ(true, true, false);

            DPRINTLN("RF24 Amp Pwr: RF24_PA_" + String(rf24AmpPower[AmplifierPower]));
            mNrf24.setPALevel(AmplifierPower & 0x03);
            mNrf24.startListening();

            DPRINTLN("Radio Config:");
            mNrf24.printPrettyDetails();

            mTxCh = getDefaultChannel();

            if(!mNrf24.isChipConnected()) {
                DPRINTLN("WARNING! your NRF24 module can't be reached, check the wiring");
            }
        }

        void handleIntr(void) {
            uint8_t pipe, len;
            packet_t *p;

            DISABLE_IRQ;
            while(mNrf24.available(&pipe)) {
                if(!mBufCtrl->full()) {
                    p = mBufCtrl->getFront();
                    memset(p->packet, 0xcc, MAX_RF_PAYLOAD_SIZE);
                    p->rxCh = mRxChLst[mRxChIdx];
                    len = mNrf24.getPayloadSize();
                    if(len > MAX_RF_PAYLOAD_SIZE)
                        len = MAX_RF_PAYLOAD_SIZE;

                    mNrf24.read(p->packet, len);
                    mBufCtrl->pushFront(p);
                }
                else {
                    bool tx_ok, tx_fail, rx_ready;
                    mNrf24.whatHappened(tx_ok, tx_fail, rx_ready); // reset interrupt status
                    mNrf24.flush_rx(); // drop the packet
                }
            }
            RESTORE_IRQ;
        }

        uint8_t getDefaultChannel(void) {
            return mTxChLst[0];
        }
        /*uint8_t getLastChannel(void) {
            return mTxChLst[mTxChIdx];
        }

        uint8_t getNxtChannel(void) {
            if(++mTxChIdx >= 4)
                mTxChIdx = 0;
            return mTxChLst[mTxChIdx];
        }*/

        void sendTimePacket(uint64_t invId, uint32_t ts) {
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
            *len = (buf[0] >> 2);
            if(*len > (MAX_RF_PAYLOAD_SIZE - 2))
                *len = MAX_RF_PAYLOAD_SIZE - 2;
            for(uint8_t i = 1; i < (*len + 1); i++) {
                buf[i-1] = (buf[i] << 1) | (buf[i+1] >> 7);
            }

            uint8_t crc = crc8(buf, *len-1);
            bool valid  = (crc == buf[*len-1]);

            //if(valid) {
                //mRxStat[(buf[9] & 0x7F)-1]++;
                //mRxChStat[(buf[9] & 0x7F)-1][rxCh & 0x7]++;
            //}
            /*else {
                DPRINT("CRC wrong: ");
                DHEX(crc);
                DPRINT(" != ");
                DHEX(buf[*len-1]);
                DPRINTLN("");
            }*/

            return valid;
        }

        bool switchRxCh(uint8_t addLoop = 0) {
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
            if(NULL != info)
                DPRINT(String(info));
            for(uint8_t i = 0; i < len; i++) {
                DHEX(buf[i]);
                DPRINT(" ");
            }
            DPRINTLN("");
        }

        bool isChipConnected(void) {
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
            //DPRINTLN("sent packet: #" + String(mSendCnt));
            //dumpBuf("SEN ", buf, len);
            if(mSerialDebug) {
                DPRINT("Transmit " + String(len) + " | ");
                dumpBuf(NULL, buf, len);
            }

            DISABLE_IRQ;
            mNrf24.stopListening();

            if(clear) {
                /*uint8_t cnt = 4;
                for(uint8_t i = 0; i < 4; i ++) {
                    DPRINT(String(mRxStat[i]) + " (");
                    for(uint8_t j = 0; j < 4; j++) {
                        DPRINT(String(mRxChStat[i][j]));
                    }
                    DPRINT(") ");
                    if(0 != mRxStat[i])
                        cnt--;
                }
                if(cnt == 0)
                    DPRINTLN(" -> all");
                else
                    DPRINTLN(" -> missing: " + String(cnt));
                memset(mRxStat, 0, 4);
                memset(mRxChStat, 0, 4*8);*/
                mRxLoopCnt = RX_LOOP_CNT;
            }

            mTxCh = getDefaultChannel();
            mNrf24.setChannel(mTxCh);

            mNrf24.openWritingPipe(invId); // TODO: deprecated
            mNrf24.setCRCLength(RF24_CRC_16);
            mNrf24.enableDynamicPayloads();
            mNrf24.setAutoAck(true);
            mNrf24.setRetries(3, 15); // 3*250us and 15 loops -> 11.25ms

            mNrf24.write(buf, len);

            // Try to avoid zero payload acks (has no effect)
            mNrf24.openWritingPipe(DUMMY_RADIO_ID); // TODO: why dummy radio id?, deprecated

            mNrf24.setAutoAck(false);
            mNrf24.setRetries(0, 0);
            mNrf24.disableDynamicPayloads();
            mNrf24.setCRCLength(RF24_CRC_DISABLED);

            mRxChIdx = 0;
            mNrf24.setChannel(mRxChLst[mRxChIdx]);
            mNrf24.startListening();

            RESTORE_IRQ;
            mSendCnt++;
        }

        uint8_t getRxNxtChannel(void) {
            if(++mRxChIdx >= 4)
                mRxChIdx = 0;
            return mRxChLst[mRxChIdx];
        }

        uint8_t mTxCh;
        uint8_t mTxChLst[1];
        //uint8_t mTxChIdx;

        uint8_t mRxChLst[4];
        uint8_t mRxChIdx;
        //uint8_t mRxStat[4];
        //uint8_t mRxChStat[4][8];
        uint16_t mRxLoopCnt;

        RF24 mNrf24;
        BUFFER *mBufCtrl;
        uint8_t mTxBuf[MAX_RF_PAYLOAD_SIZE];
};

#endif /*__RADIO_H__*/
