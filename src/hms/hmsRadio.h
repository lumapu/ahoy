//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HMS_RADIO_H__
#define __HMS_RADIO_H__

#include "cmt2300a.h"
#include "../hm/radio.h"

template<uint32_t DTU_SN = 0x81001765>
class CmtRadio : public Radio {
    typedef Cmt2300a CmtType;
    public:
        CmtRadio() {
            mDtuSn    = DTU_SN;
            mCmtAvail = false;
        }

        void setup(bool *serialDebug, bool *privacyMode, bool *printWholeTrace, uint8_t pinSclk, uint8_t pinSdio, uint8_t pinCsb, uint8_t pinFcsb, bool genDtuSn = true) {
            mCmt.setup(pinSclk, pinSdio, pinCsb, pinFcsb);
            reset(genDtuSn);
            mPrivacyMode = privacyMode;
            mSerialDebug = serialDebug;
            mPrintWholeTrace = printWholeTrace;
        }

        void loop() {
            mCmt.loop();
            if((!mIrqRcvd) && (!mRqstGetRx))
                return;
            getRx();
            if(CMT_SUCCESS == mCmt.goRx()) {
                mIrqRcvd   = false;
                mRqstGetRx = false;
            }
        }

        bool isChipConnected(void) {
            return mCmtAvail;
        }

        void sendControlPacket(Inverter<> *iv, uint8_t cmd, uint16_t *data, bool isRetransmit) {
            DPRINT(DBG_INFO, F("sendControlPacket cmd: 0x"));
            DBGHEXLN(cmd);
            initPacket(iv->radioId.u64, TX_REQ_DEVCONTROL, SINGLE_FRAME);
            uint8_t cnt = 10;

            mTxBuf[cnt++] = cmd; // cmd -> 0 on, 1 off, 2 restart, 11 active power, 12 reactive power, 13 power factor
            mTxBuf[cnt++] = 0x00;
            if(cmd >= ActivePowerContr && cmd <= PFSet) { // ActivePowerContr, ReactivePowerContr, PFSet
                mTxBuf[cnt++] = ((data[0] * 10) >> 8) & 0xff; // power limit
                mTxBuf[cnt++] = ((data[0] * 10)     ) & 0xff; // power limit
                mTxBuf[cnt++] = ((data[1]     ) >> 8) & 0xff; // setting for persistens handlings
                mTxBuf[cnt++] = ((data[1]     )     ) & 0xff; // setting for persistens handling
            }

            sendPacket(iv, cnt, isRetransmit);
        }

        bool switchFrequency(Inverter<> *iv, uint32_t fromkHz, uint32_t tokHz) {
            uint8_t fromCh = mCmt.freq2Chan(fromkHz);
            uint8_t toCh = mCmt.freq2Chan(tokHz);

            return switchFrequencyCh(iv, fromCh, toCh);
        }

        bool switchFrequencyCh(Inverter<> *iv, uint8_t fromCh, uint8_t toCh) {
            if((0xff == fromCh) || (0xff == toCh))
                return false;

            mCmt.switchChannel(fromCh);
            sendSwitchChCmd(iv, toCh);
            mCmt.switchChannel(toCh);
            return true;
        }

    private:

        void sendPacket(Inverter<> *iv, uint8_t len, bool isRetransmit, bool appendCrc16=true) {
            // inverters have maybe different settings regarding frequency
            if(mCmt.getCurrentChannel() != iv->config->frequency)
                mCmt.switchChannel(iv->config->frequency);

            updateCrcs(&len, appendCrc16);

            if(*mSerialDebug) {
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINT(F("TX "));
                DBGPRINT(String(mCmt.getFreqKhz()/1000.0f));
                DBGPRINT(F("Mhz | "));
                if(*mPrintWholeTrace) {
                    if(*mPrivacyMode)
                        ah::dumpBuf(mTxBuf, len, 1, 4);
                    else
                        ah::dumpBuf(mTxBuf, len);
                } else
                    DBGHEXLN(mTxBuf[9]);
            }

            uint8_t status = mCmt.tx(mTxBuf, len);
            mMillis = millis();
            if(CMT_SUCCESS != status) {
                DPRINT(DBG_WARN, F("CMT TX failed, code: "));
                DBGPRINTLN(String(status));
                if(CMT_ERR_RX_IN_FIFO == status)
                    mIrqRcvd = true;
            }
        }

        uint64_t getIvId(Inverter<> *iv) {
            return iv->radioId.u64;
        }

        uint8_t getIvGen(Inverter<> *iv) {
            return iv->ivGen;
        }

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

            mIrqRcvd        = false;
            mRqstGetRx      = false;
        }

        inline void sendSwitchChCmd(Inverter<> *iv, uint8_t ch) {
            /** ch:
             * 0x00: 860.00 MHz
             * 0x01: 860.25 MHz
             * 0x02: 860.50 MHz
             * ...
             * 0x14: 865.00 MHz
             * ...
             * 0x28: 870.00 MHz
             * */
            initPacket(iv->radioId.u64, 0x56, 0x02);
            mTxBuf[10] = 0x15;
            mTxBuf[11] = 0x21;
            mTxBuf[12] = ch;
            mTxBuf[13] = 0x14;
            sendPacket(iv, 14, false);
            mRqstGetRx = true;
        }

        inline void getRx(void) {
            packet_t p;
            p.millis = millis() - mMillis;
            uint8_t status = mCmt.getRx(p.packet, &p.len, 28, &p.rssi);
            if(CMT_SUCCESS == status)
                mBufCtrl.push(p);
        }

        CmtType mCmt;
        bool mRqstGetRx;
        bool mCmtAvail;
        uint32_t mMillis;
};

#endif /*__HMS_RADIO_H__*/
