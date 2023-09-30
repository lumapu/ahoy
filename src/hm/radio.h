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

// forward declaration of class
template <class REC_TYP=float>
class Inverter;

// abstract radio interface
class Radio {
    public:
        virtual void sendControlPacket(Inverter<> *iv, uint8_t cmd, uint16_t *data, bool isRetransmit, bool isNoMI = true, bool is4chMI = false) = 0;
        virtual void prepareDevInformCmd(Inverter<> *iv, uint8_t cmd, uint32_t ts, uint16_t alarmMesId, bool isRetransmit, uint8_t reqfld=TX_REQ_INFO) = 0;
        virtual void sendCmdPacket(Inverter<> *iv, uint8_t mid, uint8_t pid, bool isRetransmit, bool appendCrc16=true) = 0;
        virtual bool switchFrequency(Inverter<> *iv, uint32_t fromkHz, uint32_t tokHz) { return true; }

    protected:
        void initPacket(uint64_t ivId, uint8_t mid, uint8_t pid) {
            mTxBuf[0] = mid;
            CP_U32_BigEndian(&mTxBuf[1], ivId >> 8);
            CP_U32_LittleEndian(&mTxBuf[5], mDtuSn);
            mTxBuf[9] = pid;
            memset(&mTxBuf[10], 0x00, (MAX_RF_PAYLOAD_SIZE-10));
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
        uint8_t mTxBuf[MAX_RF_PAYLOAD_SIZE];

};

#endif /*__RADIO_H__*/
