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
    POWER_DAY,
    YIELD
};

template<class HMSYSTEM>
class HistoryData {
    private:
        struct storage_t {
            uint16_t refreshCycle = 0;
            uint16_t loopCnt = 0;
            uint16_t listIdx = 0; // index for next Element to write into WattArr
            // ring buffer for watt history
            std::array<uint16_t, (HISTORY_DATA_ARR_LENGTH + 1)> data;

            void reset() {
                loopCnt = 0;
                listIdx = 0;
                data.fill(0);
            }
        };

    public:
        void setup(IApp *app, HMSYSTEM *sys, settings_t *config, uint32_t *ts) {
            mApp = app;
            mSys = sys;
            mConfig = config;
            mTs = ts;

            mCurPwr.refreshCycle = mConfig->inst.sendInterval;
            mCurPwrDay.refreshCycle = mConfig->inst.sendInterval;
            #if defined(ENABLE_HISTORY_YIELD_PER_DAY)
            mYieldDay.refreshCycle = 60;
            #endif
            mLastValueTs = 0;
            mPgPeriod=0;
            mMaximumDay = 0;
        }

        void tickerSecond() {
            float curPwr = 0;
            //float maxPwr = 0;
            float yldDay = -0.1;
            uint32_t ts = 0;

            for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                Inverter<> *iv = mSys->getInverterByPos(i);
                record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if (iv == NULL)
                    continue;
                curPwr += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
                //maxPwr += iv->getChannelFieldValue(CH0, FLD_MP, rec);
                yldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
                if (rec->ts > ts)
                    ts = rec->ts;
            }

            if ((++mCurPwr.loopCnt % mCurPwr.refreshCycle) == 0) {
                mCurPwr.loopCnt = 0;
                if (curPwr > 0) {
                    mLastValueTs = ts;
                    addValue(&mCurPwr, roundf(curPwr));
                    if (curPwr > mMaximumDay)
                        mMaximumDay = roundf(curPwr);
                }
                //if (maxPwr > 0)
                //    mMaximumDay = roundf(maxPwr);
            }

            if ((++mCurPwrDay.loopCnt % mCurPwrDay.refreshCycle) == 0) {
                mCurPwrDay.loopCnt = 0;
                if (curPwr > 0) {
                    mLastValueTs = ts;
                    addValueDay(&mCurPwrDay, roundf(curPwr));
                }
            }

            #if defined(ENABLE_HISTORY_YIELD_PER_DAY)
            if((++mYieldDay.loopCnt % mYieldDay.refreshCycle) == 0) {
                mYieldDay.loopCnt = 0;
                if (*mTs > mApp->getSunset()) {
                    if ((!mDayStored) && (yldDay > 0)) {
                        addValue(&mYieldDay, roundf(yldDay));
                        mDayStored = true;
                    }
                } else if (*mTs > mApp->getSunrise())
                    mDayStored = false;
            }
            #endif
        }

        uint16_t valueAt(HistoryStorageType type, uint16_t i) {
            storage_t *s=NULL;
            uint16_t idx=i;
            DPRINTLN(DBG_VERBOSE, F("valueAt ") + String((uint8_t)type) + " i=" + String(i));

            idx = (s->listIdx + i) % HISTORY_DATA_ARR_LENGTH;
            switch (type) {
                default:
                    [[fallthrough]];
                case HistoryStorageType::POWER:
                    s = &mCurPwr;
                    break;
                case HistoryStorageType::POWER_DAY:
                    s = &mCurPwrDay;
                    idx = i;
                    break;
                case HistoryStorageType::YIELD:
                    s = &mYieldDay;
                    break;
            }

            return s->data[idx];
        }

        uint16_t getMaximumDay() {
            return mMaximumDay;
        }

        uint32_t getLastValueTs(HistoryStorageType type) {
            DPRINTLN(DBG_VERBOSE, F("getLastValueTs ") + String((uint8_t)type));
            if (type == HistoryStorageType::POWER_DAY)
                return mPgEndTime;
            return mLastValueTs;
        }

        uint32_t getPeriode(HistoryStorageType type) {
            DPRINTLN(DBG_VERBOSE, F("getPeriode ") + String((uint8_t)type));
            switch (type) {
                case HistoryStorageType::POWER:
                    return mCurPwr.refreshCycle;
                    break;
                case HistoryStorageType::POWER_DAY:
                    return mPgPeriod / HISTORY_DATA_ARR_LENGTH;
                    break;
                case HistoryStorageType::YIELD:
                    return (60 * 60 * 24);  // 1 day
                    break;
            }
            return 0;
        }

        bool isDataValid(void) {
            return ((0 != mPgStartTime) && (0 != mPgEndTime));
        }

        #if defined(ENABLE_HISTORY_LOAD_DATA)
        /* For filling data from outside */
        void addValue(HistoryStorageType historyType, uint8_t valueType, uint32_t value) {
            if (valueType<2) {
                storage_t *s=NULL;
                switch (historyType) {
                    default:
                        [[fallthrough]];
                    case HistoryStorageType::POWER:
                        s = &mCurPwr;
                        break;
                    case HistoryStorageType::POWER_DAY:
                        s = &mCurPwrDay;
                        break;
                    #if defined(ENABLE_HISTORY_YIELD_PER_DAY)
                    case HistoryStorageType::YIELD:
                        s = &mYieldDay;
                        break;
                    #endif
                }
                if (s)
                {
                    if (valueType==0)
                        addValue(s, value);
                    if (valueType==1)
                    {
                        if (historyType == HistoryStorageType::POWER)
                            s->refreshCycle = value;
                        if (historyType == HistoryStorageType::POWER_DAY)
                            mPgPeriod = value * HISTORY_DATA_ARR_LENGTH;
                    }
                }
                return;
            }
            if (valueType == 2)
            {
                if (historyType == HistoryStorageType::POWER)
                    mLastValueTs = value;
                if (historyType == HistoryStorageType::POWER_DAY)
                    mPgEndTime = value;
            }
        }
        #endif

    private:
        void addValue(storage_t *s, uint16_t value) {
            s->data[s->listIdx] = value;
            s->listIdx = (s->listIdx + 1) % (HISTORY_DATA_ARR_LENGTH);
        }

        void addValueDay(storage_t *s, uint16_t value) {
            DPRINTLN(DBG_VERBOSE, F("addValueDay ") + String(value));
            bool storeStartEndTimes = false;
            bool store_entry = false;
            uint32_t pGraphStartTime = mApp->getSunrise();
            uint32_t pGraphEndTime = mApp->getSunset();
            uint32_t utcTs = mApp->getTimestamp();
            switch (mPgState) {
                case PowerGraphState::NO_TIME_SYNC:
                    if ((pGraphStartTime > 0)
                        && (pGraphEndTime > 0)      // wait until period data is available ...
                        && (utcTs >= pGraphStartTime)
                        && (utcTs < pGraphEndTime)) // and current time is in period
                    {
                        storeStartEndTimes = true;  // period was received -> store
                        store_entry = true;
                        mPgState = PowerGraphState::IN_PERIOD;
                    }
                    break;
                case PowerGraphState::IN_PERIOD:
                    if (utcTs > mPgEndTime)                             // check if end of day is reached ...
                        mPgState = PowerGraphState::WAIT_4_NEW_PERIOD;  // then wait for new period setting
                    else
                        store_entry = true;
                    break;
                case PowerGraphState::WAIT_4_NEW_PERIOD:
                    if ((mPgStartTime != pGraphStartTime) || (mPgEndTime != pGraphEndTime)) { // wait until new time period was received ...
                        storeStartEndTimes = true;                                            // and store it for next period
                        mPgState = PowerGraphState::WAIT_4_RESTART;
                    }
                    break;
                case PowerGraphState::WAIT_4_RESTART:
                    if ((utcTs >= mPgStartTime) && (utcTs < mPgEndTime)) { // wait until current time is in period again ...
                        mCurPwrDay.reset();                                // then reset power graph data
                        store_entry = true;
                        mPgState = PowerGraphState::IN_PERIOD;
                        mCurPwr.reset(); // also reset "last values" graph
                        mMaximumDay = 0; // and the maximum of the (last) day
                    }
                    break;
            }

            // store start and end times of current time period and calculate period length
            if (storeStartEndTimes) {
                mPgStartTime = pGraphStartTime;
                mPgEndTime = pGraphEndTime;
                mPgPeriod = pGraphEndTime - pGraphStartTime;  // time period of power graph in sec for scaling of x-axis
            }

            if (store_entry) {
                DPRINTLN(DBG_VERBOSE, F("addValueDay store_entry") + String(value));
                if (mPgPeriod) {
                    uint16_t pgPos = (utcTs - mPgStartTime) * (HISTORY_DATA_ARR_LENGTH - 1) / mPgPeriod;
                    s->listIdx = std::min(pgPos, (uint16_t)(HISTORY_DATA_ARR_LENGTH - 1));
                } else
                    s->listIdx = 0;
                DPRINTLN(DBG_VERBOSE, F("addValueDay store_entry idx=") + String(s->listIdx));
                s->data[s->listIdx] = std::max(s->data[s->listIdx], value);  // update current datapoint to maximum of all seen values
            }
        }

    private:
        IApp *mApp = nullptr;
        HMSYSTEM *mSys = nullptr;
        settings *mSettings = nullptr;
        settings_t *mConfig = nullptr;
        uint32_t *mTs = nullptr;

        storage_t mCurPwr;
        storage_t mCurPwrDay;
        #if defined(ENABLE_HISTORY_YIELD_PER_DAY)
        storage_t mYieldDay;
        #endif
        bool mDayStored = false;
        uint16_t mMaximumDay = 0;
        uint32_t mLastValueTs = 0;
        enum class PowerGraphState {
            NO_TIME_SYNC,
            IN_PERIOD,
            WAIT_4_NEW_PERIOD,
            WAIT_4_RESTART
        };
        PowerGraphState mPgState = PowerGraphState::NO_TIME_SYNC;
        uint32_t mPgStartTime = 0;
        uint32_t mPgEndTime = 0;
        uint32_t mPgPeriod = 0;  // seconds
};

#endif /*ENABLE_HISTORY*/
#endif /*__HISTORY_DATA_H__*/
