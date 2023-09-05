#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <ESP8266HTTPClient.h>
#include <string.h>
#include "AsyncJson.h"

template <class HMSYSTEM>

class ZeroExport {
   public:
    ZeroExport() { }
    float mililine;

    void setup(cfgzeroExport_t *cfg, HMSYSTEM *sys, settings_t *config) {
        mCfg     = cfg;
        mSys     = sys;
        mConfig  = config;
    }

    void payloadEventListener(uint8_t cmd) {
        mNewPayload = true;
    }

    void tickerSecond() {
        if (mNewPayload || ((++mLoopCnt % 10) == 0)) {
            mNewPayload = false;
            mLoopCnt = 0;
            zero();
        }
    }

    double sum()
    {
        double val = mCfg->PHASE[0].power + mCfg->PHASE[1].power + mCfg->PHASE[2].power;
        if (val > 0) {
            return val;
        } else {
            float totalPower = 0;
            Inverter<> *iv;
            record_t<> *rec;
            for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                iv = mSys->getInverterByPos(i);
                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if (iv == NULL)
                    continue;
                totalPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
            }

            return totalPower - val;
        }
    }

    private:
        WiFiClient client;
        int status = WL_IDLE_STATUS;
        const uint16_t httpPort = 80;
        const String url = "/emeter/";
        char msgBuffer[256] = {'\0'};

        const String serverIp = "192.168.5.30";
        static char msg[50];

        void loop() { }
        void zero() {
            sendReq(0);
            sendReq(1);
            sendReq(2);


        }

        // TODO: change int to u_short
        void sendReq(int index) {
            if (!client.connect(serverIp, httpPort)) {
                delay(1000);
                DPRINTLN(DBG_INFO, F("connection failed"));
            }

            strcpy ( msgBuffer, "GET ");
            strcat ( msgBuffer, url.c_str());

            char str[10];
            sprintf(str, "%d", index);

            strcat ( msgBuffer, str);
            strcat ( msgBuffer, "\r\nHTTP/1.1\r\n\r\n" );
            client.print(msgBuffer);

            DynamicJsonDocument json(128);
            String raw = getData();
            deserializeJson(json, raw);

            //DPRINTLN(DBG_INFO, raw);

            mCfg->PHASE[index].power          = json[F("power")];
            mCfg->PHASE[index].pf             = json[F("pf")];
            mCfg->PHASE[index].current        = json[F("current")];
            mCfg->PHASE[index].voltage        = json[F("voltage")];
            mCfg->PHASE[index].is_valid       = json[F("is_valid")];
            mCfg->PHASE[index].total          = json[F("total")];
            mCfg->PHASE[index].total_returned = json[F("total_returned")];
        }

        String getData() {
            while (client.connected()) {
                //TODO: what if available is not true? It will stuck here then...
                while (client.available())
                {
                    String raw = client.readString();
                    int x = raw.indexOf("{", 0);
                    client.stop();
                    return raw.substring(x, raw.length());
                }
            }
            return F("error");
        }

        // private member variables
        bool mNewPayload;
        uint8_t mLoopCnt;
        const char *mVersion;
        cfgzeroExport_t *mCfg;

        settings_t *mConfig;
        HMSYSTEM *mSys;
        uint16_t mRefreshCycle;
};

#endif /*__ZEROEXPORT__*/