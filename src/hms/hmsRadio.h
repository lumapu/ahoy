//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HMS_RADIO_H__
#define __HMS_RADIO_H__

#include "../utils/dbg.h"
#include "cmt2300a.h"

typedef struct {
    int8_t rssi;
    uint8_t data[28];
} hmsPacket_t;

#define U32_B3(val) ((uint8_t)((val >> 24) & 0xff))
#define U32_B2(val) ((uint8_t)((val >> 16) & 0xff))
#define U32_B1(val) ((uint8_t)((val >>  8) & 0xff))
#define U32_B0(val) ((uint8_t)((val      ) & 0xff))

template<class SPI, uint32_t DTU_SN = 0x81001765>
class CmtRadio {
    typedef SPI SpiType;
    typedef Cmt2300a<SpiType> CmtType;
    public:
        CmtRadio() {
            mDtuSn = DTU_SN;
        }

        void setup(uint8_t pinCsb, uint8_t pinFcsb, bool genDtuSn = true) {
            mCmt.setup(pinCsb, pinFcsb);
            reset(genDtuSn);
        }

        void setup(bool genDtuSn = true) {
            mCmt.setup();
            reset(genDtuSn);
        }

        bool loop() {
            mCmt.loop();

            if(++mCnt > 30000) {
                mCnt = 0;
                if(NULL != mIvIdChannelSet)
                    prepareSwitchChannelCmd(mIvIdChannelSet);
            }

            if(!mIrqRcvd)
                return false;
            mIrqRcvd = false;
            getRx();
            mCmt.goRx();
            return true;
        }

        void tickSecond() {
        }

        void handleIntr(void) {
            mIrqRcvd = true;
        }

        void enableDebug() {
            mSerialDebug = true;
        }

        void setIvBackChannel(const uint64_t *ivId) {
            mIvIdChannelSet = ivId;
            Serial.println("Byte 3 " + String(U32_B3((*mIvIdChannelSet) >> 8), HEX));
            Serial.println("Byte 2 " + String(U32_B2((*mIvIdChannelSet) >> 8), HEX));
            Serial.println("Byte 1 " + String(U32_B1((*mIvIdChannelSet) >> 8), HEX));
            Serial.println("Byte 0 " + String(U32_B0((*mIvIdChannelSet) >> 8), HEX));
            prepareSwitchChannelCmd(mIvIdChannelSet);

        }

        void prepareDevInformCmd(const uint64_t *ivId, uint8_t cmd, uint32_t ts, uint16_t alarmMesId, bool isRetransmit, uint8_t reqfld=TX_REQ_INFO) { // might not be necessary to add additional arg.
            initPacket(ivId, reqfld, ALL_FRAMES);
            mTxBuf[10] = cmd;
            CP_U32_LittleEndian(&mTxBuf[12], ts);
            /*if (cmd == RealTimeRunData_Debug || cmd == AlarmData ) {
                mTxBuf[18] = (alarmMesId >> 8) & 0xff;
                mTxBuf[19] = (alarmMesId     ) & 0xff;
            }*/
            sendPacket(24, isRetransmit);
        }

        inline void prepareSwitchChannelCmd(const uint64_t *ivId, uint8_t freqSel = 0x0c) {
            /** freqSel:
             * 0x0c: 863.00 MHz
             * 0x0d: 863.24 MHz
             * 0x0e: 863.48 MHz
             * 0x0f: 863.72 MHz
             * 0x10: 863.96 MHz
             * */
            initPacket(ivId, 0x56, 0x02);
            mTxBuf[10] = 0x15;
            mTxBuf[11] = 0x21;
            mTxBuf[12] = freqSel;
            mTxBuf[13] = 0x14;
            sendPacket(14, false);
        }

        void sendPacket(uint8_t len, bool isRetransmit) {
            if (len > 14) {
                uint16_t crc = ah::crc16(&mTxBuf[10], len - 10);
                mTxBuf[len++] = (crc >> 8) & 0xff;
                mTxBuf[len++] = (crc     ) & 0xff;
            }
            mTxBuf[len] = ah::crc8(mTxBuf, len);
            len++;

            if(mSerialDebug) {
                DPRINT(DBG_INFO, F("TX "));
                DBGPRINT(String(len));
                DBGPRINT(F(" | "));
                ah::dumpBuf(mTxBuf, len);
            }

            uint8_t status = mCmt.tx(mTxBuf, len);
            if(CMT_SUCCESS != status) {
                DPRINT(DBG_WARN, F("CMT TX failed, code: "));
                DBGPRINTLN(String(status));
                if(CMT_ERR_RX_IN_FIFO == status)
                    mIrqRcvd = true;
            }

            if(isRetransmit)
                mRetransmits++;
            else
                mSendCnt++;
        }

        uint32_t mSendCnt;
        uint32_t mRetransmits;
        std::queue<hmsPacket_t> mBufCtrl;

    private:
        inline void reset(bool genDtuSn) {
            if(genDtuSn)
                generateDtuSn();
            if(!mCmt.reset())
                DPRINTLN(DBG_WARN, F("Initializing CMT2300A failed!"));
            else
                mCmt.goRx();

            mSendCnt        = 0;
            mRetransmits    = 0;
            mSerialDebug    = false;
            mIvIdChannelSet = NULL;
            mIrqRcvd        = false;
            mCnt            = 0;
        }

        void initPacket(const uint64_t *ivId, uint8_t mid, uint8_t pid) {
            mTxBuf[0] = mid;
            CP_U32_BigEndian(&mTxBuf[1], (*ivId) >> 8);
            CP_U32_LittleEndian(&mTxBuf[5], mDtuSn);
            mTxBuf[9] = pid;
            memset(&mTxBuf[10], 0x00, 17);
        }

        inline void generateDtuSn(void) {
            uint32_t chipID = 0;
            #ifdef ESP32
            uint64_t MAC = ESP.getEfuseMac();
            chipID = ((MAC >> 8) & 0xFF0000) | ((MAC >> 24) & 0xFF00) | ((MAC >> 40) & 0xFF);
            #endif
            mDtuSn = 0x80000000; // the first digit is an 8 for DTU production year 2022, the rest is filled with the ESP chipID in decimal
            for(int i = 0; i < 7; i++) {
                mDtuSn |= (chipID % 10) << (i * 4);
                chipID /= 10;
            }
        }

        inline void getRx(void) {
            hmsPacket_t p;
            uint8_t status = mCmt.getRx(p.data, 28, &p.rssi);
            if(CMT_SUCCESS == status)
                mBufCtrl.push(p);
            if(NULL != mIvIdChannelSet) {
                if(U32_B3((*mIvIdChannelSet) >> 8) != p.data[5])
                    return;
                if(U32_B2((*mIvIdChannelSet) >> 8) != p.data[4])
                    return;
                if(U32_B1((*mIvIdChannelSet) >> 8) != p.data[3])
                    return;
                if(U32_B0((*mIvIdChannelSet) >> 8) != p.data[2])
                    return;
                mIvIdChannelSet = NULL;
            }
        }

        CmtType mCmt;
        uint32_t mDtuSn;
        uint8_t mTxBuf[27];
        bool mSerialDebug;
        const uint64_t *mIvIdChannelSet;
        bool mIrqRcvd;

        uint16_t mCnt;
};

#endif /*__HMS_RADIO_H__*/
