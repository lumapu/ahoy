#ifndef __HOYMILES_H__
#define __HOYMILES_H__

#include <RF24.h>
#include <RF24_config.h>
#include "crc.h"

#define CHANNEL_HOP // switch between channels or use static channel to send

#define luint64_t       long long unsigned int

#define DEFAULT_RECV_CHANNEL    3
#define MAX_RF_PAYLOAD_SIZE     64
#define DTU_RADIO_ID            ((uint64_t)0x1234567801ULL)
#define DUMMY_RADIO_ID          ((uint64_t)0xDEADBEEF01ULL)

#define PACKET_BUFFER_SIZE      30


//-----------------------------------------------------------------------------
// MACROS
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
union uint64Bytes {
    uint64_t ull;
    uint8_t bytes[8];
};

typedef struct {
    uint8_t sendCh;
    uint8_t packet[MAX_RF_PAYLOAD_SIZE];
} NRF24_packet_t;


//-----------------------------------------------------------------------------
class hoymiles {
    public:
        hoymiles() {
            serial2RadioId();
            calcDtuIdCrc();

            mChannels[0] = 23;
            mChannels[1] = 40;
            mChannels[2] = 61;
            mChannels[3] = 75;
            mChanIdx = 1;

            mLastCrc = 0x0000;
            mRptCnt  = 0;
        }

        ~hoymiles() {}

        uint8_t getDefaultChannel(void) {
            return mChannels[2];
        }
        uint8_t getLastChannel(void) {
            return mChannels[mChanIdx];
        }

        uint8_t getNxtChannel(void) {
            if(++mChanIdx >= 4)
                mChanIdx = 0;
            return mChannels[mChanIdx];
        }

        void serial2RadioId(void) {
            uint64Bytes id;

            id.ull = 0ULL;
            id.bytes[4] = mAddrBytes[5];
            id.bytes[3] = mAddrBytes[4];
            id.bytes[2] = mAddrBytes[3];
            id.bytes[1] = mAddrBytes[2];
            id.bytes[0] = 0x01;

            mRadioId = id.ull;
        }

        uint8_t getTimePacket(uint8_t buf[], uint32_t ts) {
            getCmdPacket(buf, 0x15, 0x80, false);
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

        uint8_t getCmdPacket(uint8_t buf[], uint8_t mid, uint8_t cmd, bool calcCrc = true) {
            memset(buf, 0, MAX_RF_PAYLOAD_SIZE);
            buf[0] = mid; // message id
            CP_U32_BigEndian(&buf[1], (mRadioId     >> 8));
            CP_U32_BigEndian(&buf[5], (DTU_RADIO_ID >> 8));
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

        void dumpBuf(const char *info, uint8_t buf[], uint8_t len) {
            Serial.print(String(info));
            for(uint8_t i = 0; i < len; i++) {
                Serial.print(buf[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }

        uint8_t mAddrBytes[6];
        luint64_t mRadioId;

    private:
        void calcDtuIdCrc(void) {
            uint64_t addr = DTU_RADIO_ID;
            uint8_t dtuAddr[5];
            for(int8_t i = 4; i >= 0; i--) {
                dtuAddr[i] = addr;
                addr >>= 8;
            }
            mDtuIdCrc = crc16nrf24(dtuAddr, BIT_CNT(5));
        }


        uint8_t mChannels[4];
        uint8_t mChanIdx;
        uint16_t mDtuIdCrc;
        uint16_t mLastCrc;
        uint8_t mRptCnt;
};

#endif /*__HOYMILES_H__*/
