#ifndef __APP_H__
#define __APP_H__

#include <RF24.h>
#include <RF24_config.h>

#include "defines.h"
#include "main.h"

#include "CircularBuffer.h"
#include "hoymiles.h"
#include "hm1200Decode.h"


class app : public Main {
    public:
        app();
        ~app();

        void setup(const char *ssid, const char *pwd, uint32_t timeout);
        void loop(void);
        void handleIntr(void);

    private:
        void initRadio(void);
        void sendPacket(uint8_t data[], uint8_t length);

        void sendTicker(void);

        void showIndex(void);
        void showSetup(void);
        void showSave(void);
        void showCmdStatistics(void);
        void showHoymiles(void);
        void showLiveData(void);

        void saveValues(bool webSend);
        void dumpBuf(uint8_t buf[], uint8_t len);

        uint8_t mState;
        bool    mKeyPressed;

        RF24 *mRadio;
        hoymiles *mHoymiles;
        hm1200Decode *mDecoder;
        CircularBuffer<NRF24_packet_t> *mBufCtrl;
        NRF24_packet_t mBuffer[PACKET_BUFFER_SIZE];


        Ticker *mSendTicker;
        uint32_t mSendCnt;
        uint8_t mSendBuf[MAX_RF_PAYLOAD_SIZE];
        bool mFlagSend;
        uint8_t mSendChannel;

        uint32_t mCmds[6];
        uint32_t mChannelStat[4];
};

#endif /*__APP_H__*/
