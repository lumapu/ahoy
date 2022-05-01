#ifndef __RADIO_H__
#define __RADIO_H__

#include <RF24.h>
#include <RF24_config.h>
#include "crc.h"

//#define CHANNEL_HOP // switch between channels or use static channel to send

#define DEFAULT_RECV_CHANNEL    3

#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)
#define DUMMY_RADIO_ID          ((uint64_t)0xDEADBEEF01ULL)


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
        HmRadio() : mNrf24(CE_PIN, CS_PIN) {
            mChanOut[0] = 23;
            mChanOut[1] = 40;
            mChanOut[2] = 61;
            mChanOut[3] = 75;
            mChanIdx = 1;

            calcDtuCrc();

            pinCs  = CS_PIN;
            pinCe  = CE_PIN;
            pinIrq = IRQ_PIN;

            AmplifierPower = 1;
            mSendCnt       = 0;
        }
        ~HmRadio() {}

        void setup(BUFFER *ctrl) {
            //DPRINTLN("HmRadio::setup, pins: " + String(pinCs) + ", " + String(pinCe) + ", " + String(pinIrq));
            pinMode(pinIrq, INPUT_PULLUP);

            mBufCtrl = ctrl;

            mNrf24.begin(pinCe, pinCs);
            mNrf24.setAutoAck(false);
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

            mSendChannel = getDefaultChannel();

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
                    p->sendCh = mSendChannel;
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
            return mChanOut[2];
        }
        uint8_t getLastChannel(void) {
            return mChanOut[mChanIdx];
        }

        uint8_t getNxtChannel(void) {
            if(++mChanIdx >= 4)
                mChanIdx = 0;
            return mChanOut[mChanIdx];
        }

        void sendTimePacket(uint64_t invId, uint32_t ts) {
            sendCmdPacket(invId, 0x15, 0x80, false);
            mSendBuf[10] = 0x0b; // cid
            mSendBuf[11] = 0x00;
            CP_U32_LittleEndian(&mSendBuf[12], ts);
            mSendBuf[19] = 0x05;

            uint16_t crc = crc16(&mSendBuf[10], 14);
            mSendBuf[24] = (crc >> 8) & 0xff;
            mSendBuf[25] = (crc     ) & 0xff;
            mSendBuf[26] = crc8(mSendBuf, 26);

            sendPacket(invId, mSendBuf, 27);
        }

        void sendCmdPacket(uint64_t invId, uint8_t mid, uint8_t cmd, bool calcCrc = true) {
            memset(mSendBuf, 0, MAX_RF_PAYLOAD_SIZE);
            mSendBuf[0] = mid; // message id
            CP_U32_BigEndian(&mSendBuf[1], (invId  >> 8));
            CP_U32_BigEndian(&mSendBuf[5], (DTU_ID >> 8));
            mSendBuf[9]  = cmd;
            if(calcCrc) {
                mSendBuf[10] = crc8(mSendBuf, 10);
                sendPacket(invId, mSendBuf, 11);
            }
        }

        bool checkCrc(uint8_t buf[], uint8_t *len, uint8_t *rptCnt) {
            *len = (buf[0] >> 2);
            for (int16_t i = MAX_RF_PAYLOAD_SIZE - 1; i >= 0; i--) {
                buf[i] = ((buf[i] >> 7) | ((i > 0) ? (buf[i-1] << 1) : 0x00));
            }
            uint16_t crc = crc16nrf24(buf, BIT_CNT(*len + 2), 7, mDtuIdCrc);

            bool valid = (crc == ((buf[*len+2] << 8) | (buf[*len+3])));

            if(valid) {
                if(mLastCrc == crc)
                    *rptCnt = (++mRptCnt);
                else {
                    mRptCnt = 0;
                    *rptCnt = 0;
                    mLastCrc = crc;
                }
            }

            return valid;
        }

        void dumpBuf(const char *info, uint8_t buf[], uint8_t len) {
            DPRINT(String(info));
            for(uint8_t i = 0; i < len; i++) {
                if(buf[i] < 10)
                    DPRINT("0");
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

    private:
        void sendPacket(uint64_t invId, uint8_t buf[], uint8_t len) {
            //DPRINTLN("sent packet: #" + String(mSendCnt));
            //dumpBuf("SEN ", buf, len);

            DISABLE_IRQ;
            mNrf24.stopListening();

        #ifdef CHANNEL_HOP
            mSendChannel = getNxtChannel();
        #else
            mSendChannel = getDefaultChannel();
        #endif
            mNrf24.setChannel(mSendChannel);
            //DPRINTLN("CH: " + String(mSendChannel));

            mNrf24.openWritingPipe(invId); // TODO: deprecated
            mNrf24.setCRCLength(RF24_CRC_16);
            mNrf24.enableDynamicPayloads();
            mNrf24.setAutoAck(true);
            mNrf24.setRetries(3, 15);

            mNrf24.write(buf, len);

            // Try to avoid zero payload acks (has no effect)
            mNrf24.openWritingPipe(DUMMY_RADIO_ID); // TODO: why dummy radio id?, deprecated

            mNrf24.setAutoAck(false);
            mNrf24.setRetries(0, 0);
            mNrf24.disableDynamicPayloads();
            mNrf24.setCRCLength(RF24_CRC_DISABLED);

            mNrf24.setChannel(DEFAULT_RECV_CHANNEL);
            mNrf24.startListening();

            RESTORE_IRQ;
            mSendCnt++;
        }

        void calcDtuCrc(void) {
            uint64_t addr = DTU_RADIO_ID;
            uint8_t tmp[5];
            for(int8_t i = 4; i >= 0; i--) {
                tmp[i] = addr;
                addr >>= 8;
            }
            mDtuIdCrc = crc16nrf24(tmp, BIT_CNT(5));
        }

        uint8_t mChanOut[4];
        uint8_t mChanIdx;
        uint16_t mDtuIdCrc;
        uint16_t mLastCrc;
        uint8_t mRptCnt;

        RF24 mNrf24;
        uint8_t mSendChannel;
        BUFFER *mBufCtrl;
        uint8_t mSendBuf[MAX_RF_PAYLOAD_SIZE];
};

#endif /*__RADIO_H__*/
