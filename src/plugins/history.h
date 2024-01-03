#ifndef __HISTORY_DATA_H__
#define __HISTORY_DATA_H__

#include "utils/helper.h"
#include "defines.h"
#include "hm/hmSystem.h"

typedef HmSystem<MAX_NUM_INVERTERS> HmSystemType;
class IApp;

#define HISTORY_DATA_ARR_LENGTH 256

class HistoryData {
   public:
    HistoryData() {
        for (int i = 0; i < HISTORY_DATA_ARR_LENGTH; i++)
            m_dataArr[i] = 0.0;
        m_listIdx = 0;
        m_dispIdx = 0;
        m_wrapped = false;
    };
    void addValue(uint16_t value)
    {
        m_dataArr[m_listIdx] = value;
        m_listIdx = (m_listIdx + 1) % (HISTORY_DATA_ARR_LENGTH);
        if (m_listIdx == 0)
            m_wrapped = true;
        if (m_wrapped) // after 1st time array wrap we have to increas the display index
            m_dispIdx = (m_dispIdx + 1) % (HISTORY_DATA_ARR_LENGTH);
    };

    uint16_t valueAt(int i){
        uint16_t idx = m_dispIdx + i;
        idx = idx % HISTORY_DATA_ARR_LENGTH;
        uint16_t value = m_dataArr[idx];
        return value;
    };
    uint16_t getDisplIdx() { return m_dispIdx; };

   private:
    uint16_t m_dataArr[HISTORY_DATA_ARR_LENGTH + 1];  // ring buffer for watt history
    uint16_t m_listIdx;                        // index for next Element to write into WattArr
    uint16_t m_dispIdx;                        // index for 1st Element to display from WattArr
    bool m_wrapped;
};

class TotalPowerHistory : public HistoryData {
   public:
    TotalPowerHistory() : HistoryData() {
        mLoopCnt = 0;
    };

    void setup(IApp *app, HmSystemType *sys, settings_t *config);
    void tickerSecond();
    uint16_t getMaximumDay() { return mMaximumDay; }

   private:
    IApp *mApp;
    HmSystemType *mSys;
    settings *mSettings;
    settings_t *mConfig;
    uint16_t mRefreshCycle;
    uint16_t mLoopCnt;

    uint16_t mMaximumDay;
};

class YieldDayHistory : public HistoryData {
   public:
    YieldDayHistory() : HistoryData(){
        mLoopCnt = 0;
    };

    void setup(IApp *app, HmSystemType *sys, settings_t *config);
    void tickerSecond();

   private:
    IApp *mApp;
    HmSystemType *mSys;
    settings *mSettings;
    settings_t *mConfig;
    uint16_t mRefreshCycle;
    uint16_t mLoopCnt;
    bool mDayStored;
};

#endif