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

    void setup(cfgzeroExport_t *cfg, HMSYSTEM *sys, settings_t *config) {
        mCfg     = cfg;
        mSys     = sys;
        mConfig  = config;
    }

    void tickerSecond() {
        //DPRINTLN(DBG_INFO, (F("tickerSecond()")));
        if (millis() - mCfg->lastTime < mCfg->count_avg * 1000UL) {
            zero(); // just refresh when it is needed. To get cpu load low.
        }
    }

    // Sums up the power values ​​of all phases and returns them.
    // If the value is negative, all power values ​​from the inverter are taken into account
    double getPowertoSetnewValue()
    {
        float ivPower = 0;
        Inverter<> *iv;
        record_t<> *rec;
        for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
            iv = mSys->getInverterByPos(i);
            rec = iv->getRecordStruct(RealTimeRunData_Debug);
            if (iv == NULL)
                continue;
            ivPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
        }

        return ((unsigned)(mCfg->total_power - mCfg->power_avg) >= mCfg->power_avg) ?  ivPower + mCfg->total_power : ivPower - mCfg->total_power;
    }

    private:
        HTTPClient httpClient;

        // TODO: Need to improve here. 2048 for a JSON Obj is to big!?
        bool zero()
        {
            if (!httpClient.begin(mCfg->monitor_url)) {
                DPRINTLN(DBG_INFO, "httpClient.begin failed");
                httpClient.end();
                return false;
            }

            httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            httpClient.setUserAgent("Ahoy-Agent");
            httpClient.setConnectTimeout(1000);
            httpClient.setTimeout(1000);
            httpClient.addHeader("Content-Type", "application/json");
            httpClient.addHeader("Accept", "application/json");

            int httpCode = httpClient.GET();
            if (httpCode == HTTP_CODE_OK)
            {
                String responseBody = httpClient.getString();
                DynamicJsonDocument json(2048);
                DeserializationError err = deserializeJson(json, responseBody);

                // Parse succeeded?
                if (err) {
                    DPRINTLN(DBG_INFO, (F("ZeroExport() JSON error returned: ")));
                    DPRINTLN(DBG_INFO, String(err.f_str()));
                }

                // check if it HICHI
                if(json.containsKey(F("StatusSNS")) ) {
                    int index = responseBody.indexOf(String(mCfg->json_path));  // find first match position
                    responseBody = responseBody.substring(index);               // cut it and store it in value
                    index = responseBody.indexOf(",");                          // find the first seperation - Bingo!?

                    mCfg->total_power = responseBody.substring(0, index - 1).toDouble();
                } else if(json.containsKey(F("emeters"))) {
                    mCfg->total_power = (double)json[F("total_power")];
                } else {
                     DPRINTLN(DBG_INFO, (F("ZeroExport() json error: cant find value in this query: ") + responseBody));
                     return false;
                }
            }
            else
            {
                DPRINTLN(DBG_INFO, F("ZeroExport(): Error ") + String(httpCode));
                return false;
            }
            return true;
        }

        // private member variables
        cfgzeroExport_t *mCfg;
        settings_t *mConfig;
        HMSYSTEM *mSys;
};

#endif /*__ZEROEXPORT__*/

#endif /* #if defined(ESP32) */