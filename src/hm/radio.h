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
};

#endif /*__RADIO_H__*/
