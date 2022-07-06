//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __APP_H__
#define __APP_H__

#include "main.h"
#include <RF24.h>
#include <RF24_config.h>
#include <ArduinoJson.h>

#include "defines.h"

#include "CircularBuffer.h"
#include "hmSystem.h"
#include "mqtt.h"


typedef CircularBuffer<packet_t, PACKET_BUFFER_SIZE> BufferType;
typedef HmRadio<RF24_CE_PIN, RF24_CS_PIN, RF24_IRQ_PIN, BufferType> RadioType;
typedef Inverter<float> InverterType;
typedef HmSystem<RadioType, BufferType, MAX_NUM_INVERTERS, InverterType> HmSystemType;

const char* const wemosPins[] = {"D3 (GPIO0)", "TX (GPIO1)", "D4 (GPIO2)", "RX (GPIO3)",
                                "D2 (GPIO4)", "D1 (GPIO5)", "GPIO6", "GPIO7", "GPIO8",
                                "GPIO9", "GPIO10", "GPIO11", "D6 (GPIO12)", "D7 (GPIO13)",
                                "D5 (GPIO14)", "D8 (GPIO15)", "D0 (GPIO16 - no IRQ!)"};
const char* const pinNames[] = {"CS", "CE", "IRQ"};
const char* const pinArgNames[] = {"pinCs", "pinCe", "pinIrq"};


typedef struct {
    uint8_t invId;
    uint32_t ts;
    uint8_t data[MAX_PAYLOAD_ENTRIES][MAX_RF_PAYLOAD_SIZE];
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool complete;
    uint8_t maxPackId;
    uint8_t retransmits;
    bool requested;
} invPayload_t;


class app : public Main {
    public:
        app();
        ~app();

        void setup(uint32_t timeout);
        void loop(void);
        void handleIntr(void);

        uint8_t app_loops;
        uint8_t getIrqPin(void) {
            return mSys->Radio.pinIrq;
        }

    private:
        bool buildPayload(uint8_t id);
        void processPayload(bool retransmit);

        void showFavicon(void);
        void showIndex(void);
        void showSetup(void);
        void showSave(void);
        void showErase(void);
        void showStatistics(void);
        void showHoymiles(void);
        void showLiveData(void);
        void showJSON(void);


        void saveValues(bool webSend);
        void updateCrc(void);
        void sendMqttDiscoveryConfig(void);
        const char* getFieldDeviceClass(uint8_t fieldId);
        const char* getFieldStateClass(uint8_t fieldId);

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

        uint16_t mSendTicker;
        uint16_t mSendInterval;
        uint8_t mSendLastIvId;

        invPayload_t mPayload[MAX_NUM_INVERTERS];
        uint32_t mRxFailed;
        uint32_t mRxSuccess;
        uint32_t mFrameCnt;
        uint8_t mLastPacketId;
        uint8_t mMaxRetransPerPyld;

        // timer
        uint32_t mTicker;
        bool mSerialValues;
        bool mSerialDebug;

        uint32_t mRxTicker;

        // mqtt
        mqtt mMqtt;
        uint16_t mMqttTicker;
        uint16_t mMqttInterval;
        bool mMqttActive;
        bool mMqttConfigSendState[MAX_NUM_INVERTERS];

        // serial
        uint16_t mSerialTicker;
        uint16_t mSerialInterval;
};

#endif /*__APP_H__*/
