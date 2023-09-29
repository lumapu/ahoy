//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HMS_RADIO_H__
#define __HMS_RADIO_H__

#include "../utils/dbg.h"
#include "cmt2300a.h"

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
            mDtuSn    = DTU_SN;
            mCmtAvail = false;
        }

        void setup(statistics_t *stat, uint8_t pinSclk, uint8_t pinSdio, uint8_t pinCsb, uint8_t pinFcsb, bool genDtuSn = true) {
            mCmt.setup(pinSclk, pinSdio, pinCsb, pinFcsb);
            mStat = stat;
            reset(genDtuSn);
        }

        void setup(bool genDtuSn = true) {
            mCmt.setup();
            reset(genDtuSn);
        }

        bool loop() {
            mCmt.loop();

            if((!mIrqRcvd) && (!mRqstGetRx))
                return false;
            getRx();
            if(CMT_SUCCESS == mCmt.goRx()) {
                mIrqRcvd   = false;
                mRqstGetRx = false;
                return true;
            } else
                return false;
        }

        void tickSecond() {
        }

        void handleIntr(void) {
            mIrqRcvd = true;
        }

        void enableDebug() {
            mSerialDebug = true;
        }

        bool isConnected() {
            return mCmtAvail;
        }

        void sendControlPacket(const uint64_t *ivId, uint8_t cmd, uint16_t *data, bool isRetransmit) {
            DPRINT(DBG_INFO, F("sendControlPacket cmd: 0x"));
            DBGHEXLN(cmd);
            initPacket(ivId, TX_REQ_DEVCONTROL, SINGLE_FRAME);
            uint8_t cnt = 10;

            mTxBuf[cnt++] = cmd; // cmd -> 0 on, 1 off, 2 restart, 11 active power, 12 reactive power, 13 power factor
            mTxBuf[cnt++] = 0x00;
            if(cmd >= ActivePowerContr && cmd <= PFSet) { // ActivePowerContr, ReactivePowerContr, PFSet
                mTxBuf[cnt++] = ((data[0] * 10) >> 8) & 0xff; // power limit
                mTxBuf[cnt++] = ((data[0] * 10)     ) & 0xff; // power limit
                mTxBuf[cnt++] = ((data[1]     ) >> 8) & 0xff; // setting for persistens handlings
                mTxBuf[cnt++] = ((data[1]     )     ) & 0xff; // setting for persistens handling
            }

            sendPacket(cnt, isRetransmit);
        }

        bool switchFrequency(const uint64_t *ivId, uint32_t fromkHz, uint32_t tokHz) {
            uint8_t fromCh = mCmt.freq2Chan(fromkHz);
            uint8_t toCh = mCmt.freq2Chan(tokHz);

            if((0xff == fromCh) || (0xff == toCh))
                return false;

            mCmt.switchChannel(fromCh);
            sendSwitchChCmd(ivId, toCh);
            mCmt.switchChannel(toCh);
            return true;
        }

        void prepareDevInformCmd(const uint64_t *ivId, uint8_t cmd, uint32_t ts, uint16_t alarmMesId, bool isRetransmit, uint8_t reqfld=TX_REQ_INFO) { // might not be necessary to add additional arg.
            initPacket(ivId, reqfld, ALL_FRAMES);
            mTxBuf[10] = cmd;
            CP_U32_LittleEndian(&mTxBuf[12], ts);
            if (cmd == AlarmData ) { //cmd == RealTimeRunData_Debug ||
                mTxBuf[18] = (alarmMesId >> 8) & 0xff;
                mTxBuf[19] = (alarmMesId     ) & 0xff;
            }
            sendPacket(24, isRetransmit);
        }

        void sendCmdPacket(uint64_t ivId, uint8_t mid, uint8_t pid, bool isRetransmit) {
            initPacket(&ivId, mid, pid);
            sendPacket(10, isRetransmit);
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
                DBGPRINT(String(mCmt.getFreqKhz()/1000.0f));
                DBGPRINT(F("Mhz | "));
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
                mStat->retransmits++;
            else
                mStat->txCnt++;
        }

        std::queue<packet_t> mBufCtrl;

    private:
        inline void reset(bool genDtuSn) {
            if(genDtuSn)
                generateDtuSn();
            if(!mCmt.reset()) {
                mCmtAvail = false;
                DPRINTLN(DBG_WARN, F("Initializing CMT2300A failed!"));
            } else {
                mCmtAvail = true;
                mCmt.goRx();
            }

            mSerialDebug    = false;
            mIrqRcvd        = false;
            mRqstGetRx      = false;
        }

        inline void sendSwitchChCmd(const uint64_t *ivId, uint8_t ch) {
            /** ch:
             * 0x00: 860.00 MHz
             * 0x01: 860.25 MHz
             * 0x02: 860.50 MHz
             * ...
             * 0x14: 865.00 MHz
             * ...
             * 0x28: 870.00 MHz
             * */
            initPacket(ivId, 0x56, 0x02);
            mTxBuf[10] = 0x15;
            mTxBuf[11] = 0x21;
            mTxBuf[12] = ch;
            mTxBuf[13] = 0x14;
            sendPacket(14, false);
            mRqstGetRx = true;
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
            packet_t p;
            uint8_t status = mCmt.getRx(p.packet, &p.len, 28, &p.rssi);
            if(CMT_SUCCESS == status)
                mBufCtrl.push(p);
        }

        CmtType mCmt;
        uint32_t mDtuSn;
        uint8_t mTxBuf[27];
        bool mSerialDebug;
        bool mIrqRcvd;
        bool mRqstGetRx;
        bool mCmtAvail;
        statistics_t *mStat;
};

#endif /*__HMS_RADIO_H__*/
