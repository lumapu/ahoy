//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __RADIO_H__
#define __RADIO_H__

#define TX_REQ_INFO         0x15
#define TX_REQ_DEVCONTROL   0x51
#define ALL_FRAMES          0x80
#define SINGLE_FRAME        0x81

#include "../utils/dbg.h"
#include "../utils/crc.h"

// forward declaration of class
template <class REC_TYP=float>
class Inverter;

// abstract radio interface
class Radio {
    public:
        virtual void sendControlPacket(Inverter<> *iv, uint8_t cmd, uint16_t *data, bool isRetransmit) = 0;
        virtual void prepareReceive(Inverter<> *iv, uint8_t cmd, bool singleframe = false) = 0;
        virtual bool switchFrequency(Inverter<> *iv, uint32_t fromkHz, uint32_t tokHz) { return true; }
        virtual bool switchFrequencyCh(Inverter<> *iv, uint8_t fromCh, uint8_t toCh) { return true; }
        virtual bool isChipConnected(void) { return false; }

        virtual void loop(void) {};

        void handleIntr(void) {
            mIrqRcvd = true;
        }

        void sendCmdPacket(Inverter<> *iv, uint8_t mid, uint8_t pid, bool isRetransmit, bool appendCrc16=true) {
            initPacket(getIvId(iv), mid, pid);
            sendPacket(iv, 10, isRetransmit, appendCrc16);
        }

        void prepareDevInformCmd(Inverter<> *iv, uint8_t cmd, uint32_t ts, uint16_t alarmMesId, bool isRetransmit, uint8_t reqfld=TX_REQ_INFO) { // might not be necessary to add additional arg.
            if(IV_MI == getIvGen(iv)) {
                DPRINT(DBG_DEBUG, F("legacy cmd 0x"));
                DPRINTLN(DBG_DEBUG,String(cmd, HEX));
                sendCmdPacket(iv, cmd, cmd, false, false);
                return;
            }

            if(*mSerialDebug) {
                DPRINT(DBG_DEBUG, F("prepareDevInformCmd 0x"));
                DPRINTLN(DBG_DEBUG,String(cmd, HEX));
            }
            initPacket(getIvId(iv), reqfld, ALL_FRAMES);
            mTxBuf[10] = cmd;
            CP_U32_LittleEndian(&mTxBuf[12], ts);
            if (cmd == AlarmData ) { //cmd == RealTimeRunData_Debug ||
                mTxBuf[18] = (alarmMesId >> 8) & 0xff;
                mTxBuf[19] = (alarmMesId     ) & 0xff;
            }
            sendPacket(iv, 24, isRetransmit);
        }

        uint32_t getDTUSn(void) {
            return mDtuSn;
        }

    public:
        std::queue<packet_t> mBufCtrl;

    protected:
        virtual void sendPacket(Inverter<> *iv, uint8_t len, bool isRetransmit, bool appendCrc16=true) = 0;
        virtual uint64_t getIvId(Inverter<> *iv) = 0;
        virtual uint8_t getIvGen(Inverter<> *iv) = 0;

        void initPacket(uint64_t ivId, uint8_t mid, uint8_t pid) {
            mTxBuf[0] = mid;
            CP_U32_BigEndian(&mTxBuf[1], ivId >> 8);
            CP_U32_LittleEndian(&mTxBuf[5], mDtuSn);
            mTxBuf[9] = pid;
            memset(&mTxBuf[10], 0x00, (MAX_RF_PAYLOAD_SIZE-10));
        }

        void updateCrcs(uint8_t *len, bool appendCrc16=true) {
            // append crc's
            if (appendCrc16 && ((*len) > 10)) {
                // crc control data
                uint16_t crc = ah::crc16(&mTxBuf[10], (*len) - 10);
                mTxBuf[(*len)++] = (crc >> 8) & 0xff;
                mTxBuf[(*len)++] = (crc     ) & 0xff;
            }
            // crc over all
            mTxBuf[*len] = ah::crc8(mTxBuf, *len);
            (*len)++;
        }

        void generateDtuSn(void) {
            uint32_t chipID = 0;
            #ifdef ESP32
            uint64_t MAC = ESP.getEfuseMac();
            chipID = ((MAC >> 8) & 0xFF0000) | ((MAC >> 24) & 0xFF00) | ((MAC >> 40) & 0xFF);
            #else
            chipID = ESP.getChipId();
            #endif
            mDtuSn = 0x80000000; // the first digit is an 8 for DTU production year 2022, the rest is filled with the ESP chipID in decimal
            for(int i = 0; i < 7; i++) {
                mDtuSn |= (chipID % 10) << (i * 4);
                chipID /= 10;
            }
        }

        uint32_t mDtuSn;
        volatile bool mIrqRcvd;
        bool *mSerialDebug, *mPrivacyMode, *mPrintWholeTrace;
        uint8_t mTxBuf[MAX_RF_PAYLOAD_SIZE];

};

#endif /*__RADIO_H__*/
