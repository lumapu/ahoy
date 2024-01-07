
#include "plugins/history.h"

#include "appInterface.h"
#include "config/config.h"
#include "utils/dbg.h"

void TotalPowerHistory::setup(IApp *app, HmSystemType *sys, settings_t *config) {
    mApp = app;
    mSys = sys;
    mConfig = config;
    mRefreshCycle = mConfig->inst.sendInterval;
    mMaximumDay = 0;

    // Debug
    //for (uint16_t i = 0; i < HISTORY_DATA_ARR_LENGTH *1.5; i++) {
    //    addValue(i);
    //}
}

void TotalPowerHistory::tickerSecond() {
    ++mLoopCnt;
    if ((mLoopCnt % mRefreshCycle) == 0) {
        //DPRINTLN(DBG_DEBUG,F("TotalPowerHistory::tickerSecond > refreshCycle" + String(mRefreshCycle) + "|" + String(mLoopCnt) + "|" + String(mRefreshCycle % mLoopCnt));
        mLoopCnt = 0;
        float totalPower = 0;
        float totalPowerDay = 0;
        Inverter<> *iv;
        record_t<> *rec;
        for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
            iv = mSys->getInverterByPos(i);
            rec = iv->getRecordStruct(RealTimeRunData_Debug);
            if (iv == NULL)
                continue;
            totalPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
            totalPowerDay += iv->getChannelFieldValue(CH0, FLD_MP, rec);
        }
        if (totalPower > 0) {
            uint16_t iTotalPower = roundf(totalPower);
            DPRINTLN(DBG_DEBUG, F("[TotalPowerHistory]: addValue(iTotalPower)=") + String(iTotalPower));
            addValue(iTotalPower);
        }
        if (totalPowerDay > 0) {
            mMaximumDay = roundf(totalPowerDay);
        }
    }
}

void YieldDayHistory::setup(IApp *app, HmSystemType *sys, settings_t *config) {
    mApp = app;
    mSys = sys;
    mConfig = config;
    mRefreshCycle = 60;  // every minute
    mDayStored = false;
};

void YieldDayHistory::tickerSecond() {
    ++mLoopCnt;
    if ((mLoopCnt % mRefreshCycle) == 0) {
        mLoopCnt = 0;
        // check for sunset. if so store yield of day once
        uint32_t sunsetTime = mApp->getSunset();
        uint32_t sunriseTime = mApp->getSunrise();
        uint32_t currentTime = mApp->getTimestamp();
        DPRINTLN(DBG_DEBUG,F("[YieldDayHistory] current | rise | set -> ") + String(currentTime) + " | " + String(sunriseTime) + " | " + String(sunsetTime));

        if (currentTime > sunsetTime) {
            if (!mDayStored) {
                DPRINTLN(DBG_DEBUG,F("currentTime > sunsetTime ") + String(currentTime) + " > " + String(sunsetTime));
                float totalYieldDay = -0.1;
                Inverter<> *iv;
                record_t<> *rec;
                for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                    iv = mSys->getInverterByPos(i);
                    rec = iv->getRecordStruct(RealTimeRunData_Debug);
                    if (iv == NULL)
                        continue;
                    totalYieldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
                }
                if (totalYieldDay > 0) {
                    uint16_t iTotalYieldDay = roundf(totalYieldDay);
                    DPRINTLN(DBG_DEBUG,F("addValue(iTotalYieldDay)=") + String(iTotalYieldDay));
                    addValue(iTotalYieldDay);
                    mDayStored = true;
                }
            }
        } else {
            if (currentTime > sunriseTime) {
                DPRINTLN(DBG_DEBUG,F("currentTime > sunriseTime ") + String(currentTime) + " > " + String(sunriseTime));
                mDayStored = false;
            }
        }
    }
}