//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Lukas Pusch, lukas@lpusch.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <functional>
#include "llist.h"
#include "dbg.h"

namespace ah {
    typedef std::function<void()> scdCb;

    enum {SCD_SEC = 1, SCD_MIN = 60, SCD_HOUR = 3600, SCD_12H = 43200, SCD_DAY = 86400};

    struct scdEvry_s {
        scdCb c;
        uint32_t timeout;
        uint32_t reload;
        scdEvry_s() : c(NULL), timeout(0), reload(0) {}
        scdEvry_s(scdCb a, uint32_t tmt, uint32_t rl) : c(a), timeout(tmt), reload(rl) {}
    };

    struct scdAt_s {
        scdCb c;
        uint32_t timestamp;
        scdAt_s() : c(NULL), timestamp(0) {}
        scdAt_s(scdCb a, uint32_t ts) : c(a), timestamp(ts) {}
    };


    typedef node_s<scdEvry_s, scdCb, uint32_t, uint32_t> sP;
    typedef node_s<scdAt_s, scdCb, uint32_t> sPAt;
    class Scheduler {
        public:
            Scheduler() {}

            void setup() {
                mUptime     = 0;
                mTimestamp  = 0;
                mPrevMillis = millis();
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
                checkEvery();
                checkAt();

            }

            void once(scdCb c, uint32_t timeout)     { mStack.add(c, timeout, 0); }
            void every(scdCb c, uint32_t interval)   { mStack.add(c, interval, interval); }
            void onceAt(scdCb c, uint32_t timestamp) { mStackAt.add(c, timestamp); }

            void everySec(scdCb c)  { mStack.add(c, SCD_SEC,  SCD_SEC);  }
            void everyMin(scdCb c)  { mStack.add(c, SCD_MIN,  SCD_MIN);  }
            void everyHour(scdCb c) { mStack.add(c, SCD_HOUR, SCD_HOUR); }
            void every12h(scdCb c)  { mStack.add(c, SCD_12H,  SCD_12H);  }
            void everyDay(scdCb c)  { mStack.add(c, SCD_DAY,  SCD_DAY);  }

            virtual void setTimestamp(uint32_t ts) {
                mTimestamp = ts;
            }

            uint32_t getUptime(void) {
                return mUptime;
            }

            uint32_t getTimestamp(void) {
                return mTimestamp;
            }

            void stat() {
                DPRINTLN(DBG_INFO, "max fill every: " + String(mStack.getMaxFill()));
                DPRINTLN(DBG_INFO, "max fill at: " + String(mStackAt.getMaxFill()));
            }

        protected:
            uint32_t mTimestamp;

        private:
            inline void checkEvery(void) {
                bool expired;
                sP *p = mStack.getFront();
                while(NULL != p) {
                    if(mDiffSeconds >= p->d.timeout) expired = true;
                    else if((p->d.timeout--) == 0)   expired = true;
                    else                             expired = false;

                    if(expired) {
                        (p->d.c)();
                        yield();
                        if(0 == p->d.reload)
                            p = mStack.rem(p);
                        else {
                            p->d.timeout = p->d.reload - 1;
                            p = mStack.get(p);
                        }
                    }
                    else
                        p = mStack.get(p);
                }
            }

            inline void checkAt(void) {
                sPAt *p = mStackAt.getFront();
                while(NULL != p) {
                    if((p->d.timestamp) <= mTimestamp) {
                        (p->d.c)();
                        yield();
                        p = mStackAt.rem(p);
                    }
                    else
                        p = mStackAt.get(p);
                }
            }

            llist<25, scdEvry_s, scdCb, uint32_t, uint32_t> mStack;
            llist<10, scdAt_s, scdCb, uint32_t> mStackAt;
            uint32_t mMillis, mPrevMillis, mDiff;
            uint32_t mUptime;
            uint8_t mDiffSeconds;
    };
}

#endif /*__SCHEDULER_H__*/
