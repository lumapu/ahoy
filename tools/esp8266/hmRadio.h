#ifndef __RADIO_H__
#define __RADIO_H__

#include <RF24.h>
#include <RF24_config.h>
#include "crc.h"

//#define CHANNEL_HOP // switch between channels or use static channel to send

#define DEFAULT_RECV_CHANNEL    3
#define MAX_RF_PAYLOAD_SIZE     64

#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)


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
template <uint8_t CE_PIN, uint8_t CS_PIN, uint8_t IRQ_PIN, uint64_t DTU_ID=DTU_RADIO_ID>
class HmRadio {
    public:
        HmRadio() {
            //pinMode(IRQ_PIN, INPUT_PULLUP);
            //attachInterrupt(digitalPinToInterrupt(IRQ_PIN), handleIntr, FALLING);

            mSendChan[0] = 23;
            mSendChan[1] = 40;
            mSendChan[2] = 61;
            mSendChan[3] = 75;
            mChanIdx = 1;

            calcDtuCrc();
        }
        ~HmRadio() {}

        uint8_t getDefaultChannel(void) {
            return mSendChan[2];
        }
        uint8_t getLastChannel(void) {
            return mSendChan[mChanIdx];
        }

        uint8_t getNxtChannel(void) {
            if(++mChanIdx >= 4)
                mChanIdx = 0;
            return mSendChan[mChanIdx];
        }

        uint8_t getTimePacket(const uint64_t *invId, uint8_t buf[], uint32_t ts) {
            getCmdPacket(invId, buf, 0x15, 0x80, false);
            buf[10] = 0x0b; // cid
            buf[11] = 0x00;
            CP_U32_LittleEndian(&buf[12], ts);
            buf[19] = 0x05;

            uint16_t crc = crc16(&buf[10], 14);
            buf[24] = (crc >> 8) & 0xff;
            buf[25] = (crc     ) & 0xff;
            buf[26] = crc8(buf, 26);

            return 27;
        }

        uint8_t getCmdPacket(const uint64_t *invId, uint8_t buf[], uint8_t mid, uint8_t cmd, bool calcCrc = true) {
            memset(buf, 0, MAX_RF_PAYLOAD_SIZE);
            buf[0] = mid; // message id
            CP_U32_BigEndian(&buf[1], ((*invId) >> 8));
            CP_U32_BigEndian(&buf[5], (DTU_ID   >> 8));
            buf[9]  = cmd;
            if(calcCrc)
                buf[10] = crc8(buf, 10);

            return 11;
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

    protected:
        void calcDtuCrc(void) {
            uint64_t addr = DTU_RADIO_ID;
            uint8_t tmp[5];
            for(int8_t i = 4; i >= 0; i--) {
                tmp[i] = addr;
                addr >>= 8;
            }
            mDtuIdCrc = crc16nrf24(tmp, BIT_CNT(5));
        }

        uint8_t mSendChan[4];
        uint8_t mChanIdx;
        uint16_t mDtuIdCrc;
        uint16_t mLastCrc;
        uint8_t mRptCnt;
};

#endif /*__RADIO_H__*/
