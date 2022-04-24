#ifndef __APP_H__
#define __APP_H__

#include <RF24.h>
#include <RF24_config.h>

#include "defines.h"
#include "main.h"

#include "CircularBuffer.h"
#include "hmSystem.h"

#include "mqtt.h"


typedef HmRadio<RF24_CE_PIN, RF24_CS_PIN, RF24_IRQ_PIN> RadioType;
typedef CircularBuffer<packet_t, PACKET_BUFFER_SIZE> BufferType;
typedef HmSystem<RadioType, BufferType, MAX_NUM_INVERTERS, float> HmSystemType;

class app : public Main {
    public:
        app();
        ~app();

        void setup(const char *ssid, const char *pwd, uint32_t timeout);
        void loop(void);
        void handleIntr(void);

    private:
        void initRadio(void);
        void sendPacket(inverter_t *inv, uint8_t data[], uint8_t length);

        void sendTicker(void);
        void mqttTicker(void);

        void showIndex(void);
        void showSetup(void);
        void showSave(void);
        void showCmdStatistics(void);
        void showHoymiles(void);
        void showLiveData(void);
        void showMqtt(void);

        void saveValues(bool webSend);

        void dumpBuf(const char *info, uint8_t buf[], uint8_t len) {
            Serial.print(String(info));
            for(uint8_t i = 0; i < len; i++) {
                Serial.print(buf[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }

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

        uint8_t mState;
        bool    mKeyPressed;

        RF24 *mRadio;
        packet_t mBuffer[PACKET_BUFFER_SIZE];
        HmSystemType *mSys;


        Ticker *mSendTicker;
        uint32_t mSendCnt;
        uint8_t mSendBuf[MAX_RF_PAYLOAD_SIZE];
        bool mFlagSend;
        uint8_t mSendChannel;

        uint32_t mCmds[6];
        uint32_t mChannelStat[4];
        uint32_t mRecCnt;

        // mqtt
        mqtt mMqtt;
        Ticker *mMqttTicker;
        bool mMqttEvt;
};

#endif /*__APP_H__*/
