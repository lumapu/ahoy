#if defined(ESP32)

#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <HTTPClient.h>
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
        HTTPClient http;
        //char msgBuffer[256] = {'\0'};

        void loop() { }
        void zero() {
            sendReq(0);
            sendReq(1);
            sendReq(2);
        }

        void sendReq(int index)
        {
            http.begin(String(mCfg->monitor_ip), 80, "/emeter/" + String(index));

            int httpResponseCode = http.GET();
            if (httpResponseCode > 0)
            {
                DynamicJsonDocument json(128);
                deserializeJson(json, getData());
                mCfg->PHASE[index].power          = json[F("power")];
                mCfg->PHASE[index].pf             = json[F("pf")];
                mCfg->PHASE[index].current        = json[F("current")];
                mCfg->PHASE[index].voltage        = json[F("voltage")];
                mCfg->PHASE[index].is_valid       = json[F("is_valid")];
                mCfg->PHASE[index].total          = json[F("total")];
                mCfg->PHASE[index].total_returned = json[F("total_returned")];
            }
            else
            {
                DPRINT(DBG_INFO, "Error code: ");
                DPRINTLN(DBG_INFO, String(httpResponseCode));
            }

        }

        String getData()
        {
            String payload = http.getString();
            int x = payload.indexOf("{", 0);
            return payload.substring(x, payload.length());
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

#endif /* #if defined(ESP32) */