//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HISTORY_DATA_H__
#define __HISTORY_DATA_H__

#if defined(ENABLE_HISTORY)

#include <array>
#include "../appInterface.h"
#include "../hm/hmSystem.h"
#include "../utils/helper.h"

#define HISTORY_DATA_ARR_LENGTH 256

enum class HistoryStorageType : uint8_t {
    POWER,
    YIELD
};

template<class HMSYSTEM>
class HistoryData {
    private:
        struct storage_t {
            uint16_t refreshCycle = 0;
            uint16_t loopCnt = 0;
            uint16_t listIdx = 0; // index for next Element to write into WattArr
            uint16_t dispIdx = 0; // index for 1st Element to display from WattArr
            bool wrapped = false;
            // ring buffer for watt history
            std::array<uint16_t, (HISTORY_DATA_ARR_LENGTH + 1)> data;

            storage_t() { data.fill(0); }
        };

    public:
        void setup(IApp *app, HMSYSTEM *sys, settings_t *config, uint32_t *ts) {
            mApp = app;
            mSys = sys;
            mConfig = config;
            mTs = ts;

            mCurPwr.refreshCycle = mConfig->inst.sendInterval;
            //mYieldDay.refreshCycle = 60;
        }

        void tickerSecond() {
            ;
            float curPwr = 0;
            float maxPwr = 0;
            float yldDay = -0.1;
            for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                Inverter<> *iv = mSys->getInverterByPos(i);
                record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if (iv == NULL)
                    continue;
                curPwr += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
                maxPwr += iv->getChannelFieldValue(CH0, FLD_MP, rec);
                yldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
            }

            if ((++mCurPwr.loopCnt % mCurPwr.refreshCycle) == 0) {
                mCurPwr.loopCnt = 0;
                if (curPwr > 0)
                    addValue(&mCurPwr, roundf(curPwr));
                if (maxPwr > 0)
                    mMaximumDay = roundf(maxPwr);
            }

            /*if((++mYieldDay.loopCnt % mYieldDay.refreshCycle) == 0) {
                if (*mTs > mApp->getSunset()) {
                    if ((!mDayStored) && (yldDay > 0)) {
                        addValue(&mYieldDay, roundf(yldDay));
                        mDayStored = true;
                    }
                } else if (*mTs > mApp->getSunrise())
                    mDayStored = false;
            }*/
        }

        uint16_t valueAt(HistoryStorageType type, uint16_t i) {
            //storage_t *s = (HistoryStorageType::POWER == type) ? &mCurPwr : &mYieldDay;
            storage_t *s = &mCurPwr;
            uint16_t idx = (s->dispIdx + i) % HISTORY_DATA_ARR_LENGTH;
            return s->data[idx];
        }

        uint16_t getMaximumDay() {
            return mMaximumDay;
        }

    private:
        void addValue(storage_t *s, uint16_t value) {
            if (s->wrapped) // after 1st time array wrap we have to increase the display index
                s->dispIdx = (s->listIdx + 1) % (HISTORY_DATA_ARR_LENGTH);
            s->data[s->listIdx] = value;
            s->listIdx = (s->listIdx + 1) % (HISTORY_DATA_ARR_LENGTH);
            if (s->listIdx == 0)
                s->wrapped = true;
        }

    private:
        IApp *mApp = nullptr;
        HMSYSTEM *mSys = nullptr;
        settings *mSettings = nullptr;
        settings_t *mConfig = nullptr;
        uint32_t *mTs = nullptr;

        storage_t mCurPwr;
        bool mDayStored = false;
        uint16_t mMaximumDay = 0;
};

#endif /*ENABLE_HISTORY*/
#endif /*__HISTORY_DATA_H__*/
