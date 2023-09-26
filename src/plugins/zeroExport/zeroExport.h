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
            if (httpResponseCode > 0 && httpResponseCode < 400)
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
            if (httpResponseCode >= 400)
            {
                DPRINTLN(DBG_INFO, "Shelly(): Error " + String(httpResponseCode));
            }
            return 2;
        }

        int Hichi()
        {
            http.begin(String(mCfg->monitor_ip), 80, "/cm?cmnd=status%208");
            String hName = String(mCfg->HICHI_PowerName);

            int httpResponseCode = http.GET();
            if (httpResponseCode > 0 && httpResponseCode < 400)
            {
                DynamicJsonDocument json(2048);
                DeserializationError err = deserializeJson(json, http.getString());

                // Parse succeeded?
                if (err) {
                    DPRINT(DBG_INFO, (F("Hichi() returned: ")));
                    DPRINTLN(DBG_INFO, String(err.f_str()));
                    return 0;
                }

                int count = 0;
                for (uint8_t i = 0; i < strlen(hName.c_str()); i++)
                    if (hName[i] == ']') count++;

                for (uint8_t i = 0; i < count; i++)
                {
                    uint8_t l_index = hName.indexOf(']') - 1;
                    json = json[hName.substring(1, l_index)];


                    String output;
                    serializeJson(json, output);
                    DPRINTLN(DBG_INFO, output);
                    hName = hName.substring(l_index);
                }

                DPRINTLN(DBG_INFO, String(json[0]));
                mCfg->total_power = (double)json[0];
                return 2;
            }
            if (httpResponseCode >= 400)
            {
                DPRINTLN(DBG_INFO, "HICHI(): Error " + String(httpResponseCode));
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