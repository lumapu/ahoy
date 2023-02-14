//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
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
        char name[6];
        sP() : c(NULL), timeout(0), reload(0), isTimestamp(false), name("\n") {}
        sP(scdCb a, uint32_t tmt, uint32_t rl, bool its) : c(a), timeout(tmt), reload(rl), isTimestamp(its), name("\n") {}
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
                resetTicker();
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

            void once(scdCb c, uint32_t timeout, const char *name)     { addTicker(c, timeout, 0, false, name); }
            void onceAt(scdCb c, uint32_t timestamp, const char *name) { addTicker(c, timestamp, 0, true, name); }
            uint8_t every(scdCb c, uint32_t interval, const char *name){ return addTicker(c, interval, interval, false, name); }

            void everySec(scdCb c, const char *name)  { every(c, SCD_SEC, name);  }
            void everyMin(scdCb c, const char *name)  { every(c, SCD_MIN, name);  }
            void everyHour(scdCb c, const char *name) { every(c, SCD_HOUR, name); }
            void every12h(scdCb c, const char *name)  { every(c, SCD_12H, name);  }
            void everyDay(scdCb c, const char *name)  { every(c, SCD_DAY, name);  }

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

            inline void resetTicker(void) {
                for (uint8_t i = 0; i < MAX_NUM_TICKER; i++)
                    mTickerInUse[i] = false;
            }

            void getStat(uint8_t *max) {
                *max = mMax;
            }

            void printSchedulers() {
                for (uint8_t i = 0; i < MAX_NUM_TICKER; i++) {
                    if (mTickerInUse[i]) {
                        DPRINT(DBG_INFO, String(mTicker[i].name));
                        DBGPRINT(", tmt: ");
                        DBGPRINT(String(mTicker[i].timeout));
                        DBGPRINT(", rel: ");
                        DBGPRINTLN(String(mTicker[i].reload));
                    }
                }
            }

        protected:
            uint32_t mTimestamp;

        private:
            inline uint8_t addTicker(scdCb c, uint32_t timeout, uint32_t reload, bool isTimestamp, const char *name) {
                for (uint8_t i = 0; i < MAX_NUM_TICKER; i++) {
                    if (!mTickerInUse[i]) {
                        mTickerInUse[i] = true;
                        mTicker[i].c = c;
                        mTicker[i].timeout = timeout;
                        mTicker[i].reload = reload;
                        mTicker[i].isTimestamp = isTimestamp;
                        memset(mTicker[i].name, 0, 6);
                        strncpy(mTicker[i].name, name, (strlen(name) < 6) ? strlen(name) : 5);
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
                            //DPRINTLN(DBG_INFO, "checkTick " + String(i) + " reload: " + String(mTicker[i].reload) + ", timeout: " + String(mTicker[i].timeout));
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
