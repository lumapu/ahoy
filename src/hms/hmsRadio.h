//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
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
            mDtuSn = DTU_SN;
        }

        void setup(bool *serialDebug, bool *privacyMode, bool *printWholeTrace, uint8_t pinSclk, uint8_t pinSdio, uint8_t pinCsb, uint8_t pinFcsb, uint8_t region = 0, bool genDtuSn = true) {
            mCmt.setup(pinSclk, pinSdio, pinCsb, pinFcsb);
            reset(genDtuSn, static_cast<RegionCfg>(region));
            mPrivacyMode = privacyMode;
            mSerialDebug = serialDebug;
            mPrintWholeTrace = printWholeTrace;
        }

        bool loop() {
            mCmt.loop();
            if((!mIrqRcvd) && (!mRqstGetRx))
                return false;
            getRx();
            if(CmtStatus::SUCCESS == mCmt.goRx()) {
                mIrqRcvd   = false;
                mRqstGetRx = false;
            }
            return false;
        }

        bool isChipConnected(void) const {
            return mCmtAvail;
        }

        void sendControlPacket(Inverter<> *iv, uint8_t cmd, uint16_t *data, bool isRetransmit) {
            DPRINT(DBG_INFO, F("sendControlPacket cmd: "));
            DBGHEXLN(cmd);
            initPacket(iv->radioId.u64, TX_REQ_DEVCONTROL, SINGLE_FRAME);
            uint8_t cnt = 10;

            mTxBuf[cnt++] = cmd; // cmd -> 0 on, 1 off, 2 restart, 11 active power, 12 reactive power, 13 power factor
            mTxBuf[cnt++] = 0x00;
            if(cmd >= ActivePowerContr && cmd <= PFSet) { // ActivePowerContr, ReactivePowerContr, PFSet
                mTxBuf[cnt++] = (data[0] >> 8) & 0xff; // power limit, multiplied by 10 (because of fraction)
                mTxBuf[cnt++] = (data[0]     ) & 0xff; // power limit
                mTxBuf[cnt++] = (data[1] >> 8) & 0xff; // setting for persistens handlings
                mTxBuf[cnt++] = (data[1]     ) & 0xff; // setting for persistens handling
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

        uint16_t getBaseFreqMhz(void) override {
            return mCmt.getBaseFreqMhz();
        }

        uint16_t getBootFreqMhz(void) override {
            return mCmt.getBootFreqMhz();
        }

        std::pair<uint16_t,uint16_t> getFreqRangeMhz(void) override {
            return mCmt.getFreqRangeMhz();
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
                } else {
                    DHEX(mTxBuf[0]);
                    DBGPRINT(F(" "));
                    DHEX(mTxBuf[10]);
                    DBGPRINT(F(" "));
                    DBGHEXLN(mTxBuf[9]);
                }
            }

            CmtStatus status = mCmt.tx(mTxBuf, len);
            mMillis = millis();
            if(CmtStatus::SUCCESS != status) {
                DPRINT(DBG_WARN, F("CMT TX failed, code: "));
                DBGPRINTLN(String(static_cast<uint8_t>(status)));
                if(CmtStatus::ERR_RX_IN_FIFO == status)
                    mIrqRcvd = true;
            }
            iv->mDtuTxCnt++;
        }

        uint64_t getIvId(Inverter<> *iv) const {
            return iv->radioId.u64;
        }

        uint8_t getIvGen(Inverter<> *iv) const {
            return iv->ivGen;
        }

        inline void reset(bool genDtuSn, RegionCfg region) {
            if(genDtuSn)
                generateDtuSn();
            if(!mCmt.reset(region)) {
                mCmtAvail = false;
                DPRINTLN(DBG_WARN, F("Initializing CMT2300A failed!"));
            } else {
                mCmtAvail = true;
                mCmt.goRx();
            }

            mIrqRcvd   = false;
            mRqstGetRx = false;
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
            CmtStatus status = mCmt.getRx(p.packet, &p.len, 28, &p.rssi);
            if(CmtStatus::SUCCESS == status)
                mBufCtrl.push(p);

            // this code completly stops communication!
            //if(p.packet[9] > ALL_FRAMES)          // indicates last frame
            //    mRadioWaitTime.stopTimeMonitor(); // we got everything we expected and can exit rx mode...
            //optionally instead: mRadioWaitTime.startTimeMonitor(DURATION_PAUSE_LASTFR); // let the inverter first get back to rx mode?
        }

        CmtType mCmt;
        bool mCmtAvail = false;
        bool mRqstGetRx = false;
        uint32_t mMillis;
};

#endif /*__HMS_RADIO_H__*/
