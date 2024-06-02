//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __MAX_VALUE__
#define __MAX_VALUE__
#pragma once

#include <array>
#include <utility>
#include "../hm/hmDefines.h"

template<class T=float>
class MaxPower {
    public:
        MaxPower() {
            mTs = nullptr;
            mMaxDiff = 60;
            reset();
        }

        void setup(uint32_t *ts, uint16_t interval) {
            mTs = ts;
            mMaxDiff = interval * 4;
        }

        void reset(void) {
            mValues.fill(std::make_pair(0, 0.0));
            mLast = 0.0;
        }

        void payloadEvent(uint8_t cmd, Inverter<> *iv) {
            if(RealTimeRunData_Debug != cmd)
                return;

            if(nullptr == iv)
                return;

            if(iv->id >= MAX_NUM_INVERTERS)
                return;

            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            mValues[iv->id] = std::make_pair(*mTs, iv->getChannelFieldValue(CH0, FLD_PAC, rec));
        }

        T getTotalMaxPower(void) {
            T val = 0;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                if((mValues[i].first + mMaxDiff) >= *mTs)
                    val += mValues[i].second;
                else if(mValues[i].first > 0)
                    return mLast; // old data
            }
            if(val > mLast)
                mLast = val;
            return mLast;
        }

    private:
        uint32_t *mTs;
        uint32_t mMaxDiff;
        float mLast;
        std::array<std::pair<uint32_t, T>, MAX_NUM_INVERTERS> mValues;
};

#endif
