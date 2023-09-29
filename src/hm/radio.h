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

// abstract radio interface
class Radio {
    public:
        virtual void sendControlPacket(uint64_t invId, uint8_t cmd, uint16_t *data, bool isRetransmit, bool isNoMI = true, bool is4chMI = false) = 0;
        virtual void prepareDevInformCmd(uint64_t invId, uint8_t cmd, uint32_t ts, uint16_t alarmMesId, bool isRetransmit, uint8_t reqfld=TX_REQ_INFO) = 0;
        virtual void sendCmdPacket(uint64_t invId, uint8_t mid, uint8_t pid, bool isRetransmit, bool appendCrc16=true) = 0;
        virtual bool switchFrequency(uint64_t ivId, uint32_t fromkHz, uint32_t tokHz) { return true; }
};

#endif /*__RADIO_H__*/
