#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <ESP8266HTTPClient.h>

template <class HMSYSTEM>

class ZeroExport {
   public:
    ZeroExport() { }

    WiFiClientSecure client;
    const uint8_t fingerprint[20] = {0x5A, 0xCF, 0xFE, 0xF0, 0xF1, 0xA6, 0xF4, 0x5F, 0xD2, 0x11, 0x11, 0xC6, 0x1D, 0x2F, 0x0E, 0xBC, 0x39, 0x8D, 0x50, 0xE0};

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

    private:
        void zero() {
            char host[20];
            const char* meter = "/emeter/0";
            sprintf(host, mCfg->monitor_ip, meter);

            std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
            client->setFingerprint(fingerprint);
            HTTPClient https;

            DPRINTLN(DBG_INFO, host);
            if (https.begin(*client, host))   // HTTPS
            {
                DPRINTLN(DBG_INFO, F("[HTTPS] GET...\n"));
                // start connection and send HTTP header
                int httpCode = https.GET();

                // httpCode will be negative on error
                if (httpCode > 0) {
                    // file found at server
                    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                    String payload = https.getString();
                    DPRINTLN(DBG_INFO, payload);
                    }
                } else {
                    DPRINTLN(DBG_INFO, https.errorToString(httpCode).c_str());
                }

                https.end();
            }
            else
            {
                DPRINTLN(DBG_INFO, F("[HTTPS] Unable to connect\n"));
            }
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