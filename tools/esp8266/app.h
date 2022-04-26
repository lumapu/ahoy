#ifndef __APP_H__
#define __APP_H__

#include <RF24.h>
#include <RF24_config.h>

#include "defines.h"
#include "main.h"

#include "CircularBuffer.h"
#include "hmSystem.h"
#include "mqtt.h"

typedef CircularBuffer<packet_t, PACKET_BUFFER_SIZE> BufferType;
typedef HmRadio<RF24_CE_PIN, RF24_CS_PIN, RF24_IRQ_PIN, BufferType> RadioType;
typedef HmSystem<RadioType, BufferType, MAX_NUM_INVERTERS, float> HmSystemType;

const char* const wemosPins[] = {"D3 (GPIO0)", "TX (GPIO1)", "D4 (GPIO2)", "RX (GPIO3)",
                                "D2 (GPIO4)", "D1 (GPIO5)", "GPIO6", "GPIO7", "GPIO8",
                                "GPIO9", "GPIO10", "GPIO11", "D6 (GPIO12)", "D7 (GPIO13)",
                                "D5 (GPIO14)", "D8 (GPIO15)", "D0 (GPIO16)"};
const char* const pinNames[] = {"CS", "CE", "IRQ"};
const char* const pinArgNames[] = {"pinCs", "pinCe", "pinIrq"};

const uint8_t dbgCmds[] = {0x01, 0x02, 0x03, 0x81, 0x82, 0x83, 0x84};
#define DBG_CMD_LIST_LEN    7

class app : public Main {
    public:
        app();
        ~app();

        void setup(const char *ssid, const char *pwd, uint32_t timeout);
        void loop(void);
        void handleIntr(void);

        uint8_t getIrqPin(void) {
            return mSys->Radio.pinIrq;
        }

    private:
        void sendTicker(void);
        void mqttTicker(void);

        void showIndex(void);
        void showSetup(void);
        void showSave(void);
        void showErase(void);
        void showStatistics(void);
        void showHoymiles(void);
        void showLiveData(void);
        void showMqtt(void);

        void saveValues(bool webSend);
        void updateCrc(void);

        uint64_t Serial2u64(const char *val) {
            char tmp[3] = {0};
            uint64_t ret = 0ULL;
            uint64_t u64;
            for(uint8_t i = 0; i < 6; i++) {
                tmp[0] = val[i*2];
                tmp[1] = val[i*2 + 1];
                if((tmp[0] == '\0') || (tmp[1] == '\0'))
                    break;
                u64 = strtol(tmp, NULL, 16);
                ret |= (u64 << ((5-i) << 3));
            }
            return ret;
        }

        bool mShowRebootRequest;

        HmSystemType *mSys;

        Ticker *mSendTicker;
        bool mFlagSend;

        uint32_t mCmds[DBG_CMD_LIST_LEN+1];
        //uint32_t mChannelStat[4];
        uint32_t mRecCnt;

        // mqtt
        mqtt mMqtt;
        Ticker *mMqttTicker;
        bool mMqttEvt;
};

#endif /*__APP_H__*/
