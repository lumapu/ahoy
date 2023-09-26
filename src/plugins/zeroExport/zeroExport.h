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

    void tickerSecond() {
        //DPRINTLN(DBG_INFO, (F("tickerSecond()")));
        if (millis() - mCfg->lastTime < mCfg->count_avg * 1000UL) {
            zero(); // just refresh when it is needed. To get cpu load low.
            //DPRINTLN(DBG_INFO, (F("zero()")));
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
        HTTPClient http;

        // TODO: Need to improve here. 2048 for a JSON Obj is to big!?
        void zero() {
            switch (mCfg->device) {
                case 0:
                case 1:
                    mCfg->device = Shelly();
                    break;
                case 2:
                    mCfg->device = Hichi();
                    break;
                default:
                    // Statement(s)
                    break; // Wird nicht benötigt, wenn Statement(s) vorhanden sind
            }
        }

        int Shelly()
        {
            http.begin(String(mCfg->monitor_ip), 80, "/status");

            int httpResponseCode = http.GET();
            if (httpResponseCode > 0)
            {
                DynamicJsonDocument json(2048);
                DeserializationError err = deserializeJson(json, http.getString());

                // Parse succeeded?
                if (err) {
                    DPRINTLN(DBG_INFO, (F("Shelly() returned: ")));
                    DPRINTLN(DBG_INFO, String(err.f_str()));
                    return 2;
                }

                mCfg->total_power = (double)json[F("total_power")];
                return 1;
            }
            return 2;
        }

        int Hichi()
        {
            http.begin(String(mCfg->monitor_ip), 80, "/cm?cmnd=status%208");
            int httpResponseCode = http.GET();
            if (httpResponseCode > 0)
            {
                DynamicJsonDocument json(2048);
                DeserializationError err = deserializeJson(json, http.getString());

                // Parse succeeded?
                if (err) {
                    DPRINTLN(DBG_INFO, (F("Hichi() returned: ")));
                    DPRINTLN(DBG_INFO, String(err.f_str()));
                    return 0;
                }

                mCfg->total_power = (double)json["StatusSNS"]["ENERGY"]["Power"];
                return 2;
            }
            return 0;
        }

        // private member variables
        const char *mVersion;
        cfgzeroExport_t *mCfg;

        settings_t *mConfig;
        HMSYSTEM *mSys;
        uint16_t mRefreshCycle;
};

#endif /*__ZEROEXPORT__*/

#endif /* #if defined(ESP32) */