#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <ESP8266HTTPClient.h>

template <class HMSYSTEM>

class ZeroExport {
   public:
    ZeroExport() { }

    void setup(cfgzeroExport_t *cfg, HMSYSTEM *sys, settings_t *config) {
        mCfg    = cfg;
        mSys    = sys;
        mConfig = config;
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

    void tickerMinute() {
        zero();
    }

    private:
        void zero() {
            char meter[] = "192.168.5.30/emeter/0";

            WiFiClient client;
            HTTPClient http;

            DPRINTLN(DBG_INFO, meter);

            http.begin(client, meter);   // HTTPS
            http.GET();

            // Parse response
            DynamicJsonDocument doc(2048);
            deserializeJson(doc, http.getStream());

            // Read values
            DPRINTLN(DBG_INFO, doc["power"].as<String>());

            // httpCode will be negative on error
            /*if (httpCode > 0) {
                // file found at server
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                String payload = http.getString();
                DPRINTLN(DBG_INFO, payload);
                }
            } else {
                DPRINTLN(DBG_INFO, http.errorToString(httpCode).c_str());
            }*/

            http.end();
        }

        // private member variables
        bool mNewPayload;
        uint8_t mLoopCnt;
        uint32_t *mUtcTs;
        const char *mVersion;
        cfgzeroExport_t *mCfg;

        settings_t *mConfig;
        HMSYSTEM *mSys;
        uint16_t mRefreshCycle;
};

#endif /*__ZEROEXPORT__*/