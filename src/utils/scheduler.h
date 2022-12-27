//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Lukas Pusch, lukas@lpusch.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <functional>
#include "dbg.h"

namespace ah {
    typedef std::function<void()> scdCb;

    enum {SCD_SEC = 1, SCD_MIN = 60, SCD_HOUR = 3600, SCD_12H = 43200, SCD_DAY = 86400};

    struct sP {
        scdCb c;
        uint32_t timeout;
        uint32_t reload;
        bool isTimestamp;
        sP() : c(NULL), timeout(0), reload(0), isTimestamp(false) {}
        sP(scdCb a, uint32_t tmt, uint32_t rl, bool its) : c(a), timeout(tmt), reload(rl), isTimestamp(its) {}
    };

    #define MAX_NUM_TICKER  30

    class Scheduler {
        public:
            Scheduler() {}

            void setup() {
                mUptime     = 0;
                mTimestamp  = 0;
                mMax        = 0;
                mPrevMillis = millis();
                for (uint8_t i = 0; i < MAX_NUM_TICKER; i++)
                    mTickerInUse[i] = false;
            }

            void loop(void) {
                mMillis = millis();
                mDiff = mMillis - mPrevMillis;
                if (mDiff < 1000)
                    return;

                mDiffSeconds = 1;
                if (mDiff < 2000)
                    mPrevMillis += 1000;
                else {
                    if (mMillis < mPrevMillis) { // overflow
                        mDiff = mMillis;
                        if (mDiff < 1000)
                            return;
                    }
                    mDiffSeconds = mDiff / 1000;
                    mPrevMillis += (mDiffSeconds * 1000);
                }

                mUptime += mDiffSeconds;
                if(0 != mTimestamp)
                    mTimestamp += mDiffSeconds;
                checkTicker();

            }

            void once(scdCb c, uint32_t timeout)     { addTicker(c, timeout, 0, false); }
            void onceAt(scdCb c, uint32_t timestamp) { addTicker(c, timestamp, 0, true); }
            uint8_t every(scdCb c, uint32_t interval){ return addTicker(c, interval, interval, false); }

            void everySec(scdCb c)  { every(c, SCD_SEC);  }
            void everyMin(scdCb c)  { every(c, SCD_MIN);  }
            void everyHour(scdCb c) { every(c, SCD_HOUR); }
            void every12h(scdCb c)  { every(c, SCD_12H);  }
            void everyDay(scdCb c)  { every(c, SCD_DAY);  }

            virtual void setTimestamp(uint32_t ts) {
                mTimestamp = ts;
            }

            bool resetEveryById(uint8_t id) {
                if (mTickerInUse[id] == false)
                    return false;
                mTicker[id].timeout = mTicker[id].reload;
                return true;
            }

            uint32_t getUptime(void) {
                return mUptime;
            }

            uint32_t getTimestamp(void) {
                return mTimestamp;
            }

            void getStat(uint8_t *max) {
                *max = mMax;
            }

        protected:
            uint32_t mTimestamp;

        private:
            inline uint8_t addTicker(scdCb c, uint32_t timeout, uint32_t reload, bool isTimestamp) {
                for (uint8_t i = 0; i < MAX_NUM_TICKER; i++) {
                    if (!mTickerInUse[i]) {
                        mTickerInUse[i] = true;
                        mTicker[i].c = c;
                        mTicker[i].timeout = timeout;
                        mTicker[i].reload = reload;
                        mTicker[i].isTimestamp = isTimestamp;
                        if(mMax == i)
                            mMax = i + 1;
                        return i;
                    }
                }
                return 0xff;
            }

            inline void checkTicker(void) {
                bool inUse[MAX_NUM_TICKER];
                for (uint8_t i = 0; i < MAX_NUM_TICKER; i++)
                    inUse[i] = mTickerInUse[i];
                for (uint8_t i = 0; i < MAX_NUM_TICKER; i++) {
                    if (inUse[i]) {
                        if (mTicker[i].timeout <= ((mTicker[i].isTimestamp) ? mTimestamp : mDiffSeconds)) { // expired
                            if(0 == mTicker[i].reload)
                                mTickerInUse[i] = false;
                            else
                                mTicker[i].timeout = mTicker[i].reload;
                            (mTicker[i].c)();
                            yield();
                        }
                        else // not expired
                            if (!mTicker[i].isTimestamp)
                                mTicker[i].timeout -= mDiffSeconds;
                    }
                }
            }

            sP mTicker[MAX_NUM_TICKER];
            bool mTickerInUse[MAX_NUM_TICKER];
            uint32_t mMillis, mPrevMillis, mDiff;
            uint32_t mUptime;
            uint8_t mDiffSeconds;
            uint8_t mMax;
    };
}

#endif /*__SCHEDULER_H__*/
