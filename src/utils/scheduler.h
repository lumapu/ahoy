//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Lukas Pusch, lukas@lpusch.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <memory>
#include <functional>
#include <list>

enum {EVERY_SEC = 1, EVERY_MIN, EVERY_HR, EVERY_12H, EVERY_DAY};
typedef std::function<void()> SchedulerCb;

struct once_t {
    uint32_t n;
    SchedulerCb f;
    once_t(uint32_t a, SchedulerCb b) : n(a), f(b) {}
    once_t() : n(0), f(NULL) {}
};

namespace ah {
class Scheduler {
    public:
        Scheduler() {}

        void setup() {
            mPrevMillis = 0;
            mSeconds    = 0;
            mMinutes    = 0;
            mHours      = 0;
            mUptime     = 0;
            mTimestamp  = 0;
        }

        void loop() {
            if (millis() - mPrevMillis >= 1000) {
                mPrevMillis += 1000;
                mUptime++;
                if(0 != mTimestamp)
                    mTimestamp++;
                notify(&mListSecond);
                onceFuncTick();
                if(++mSeconds >= 60) {
                    mSeconds = 0;
                    notify(&mListMinute);
                    onceAtFuncTick();
                    if(++mMinutes >= 60) {
                        mMinutes = 0;
                        notify(&mListHour);
                        if(++mHours >= 24) {
                            mHours = 0;
                            notify(&mListDay);
                            notify(&mList12h);
                        }
                        else if(mHours == 12)
                            notify(&mList12h);
                    }
                }
            }
        }

        // checked every second
        void once(uint32_t sec, SchedulerCb cb, const char *info = NULL) {
            if(NULL != info) {
                DPRINT(DBG_INFO, F("once in [s]: ") + String(sec));
                DBGPRINTLN(F(", ") + String(info));
            }
            mOnce.push_back(once_t(sec, cb));
        }

        // checked every minute
        void onceAt(uint32_t timestamp, SchedulerCb cb, const char *info = NULL) {
            if(timestamp > mTimestamp) {
                if(NULL != info) {
                    DPRINT(DBG_INFO, F("onceAt (UTC): ") + getDateTimeStr(timestamp));
                    DBGPRINTLN(F(", ") + String(info));
                }
                mOnceAt.push_back(once_t(timestamp, cb));
            }
        }

        virtual void setTimestamp(uint32_t ts) {
            mTimestamp = ts;
        }

        void addListener(uint8_t every, SchedulerCb cb) {
            switch(every) {
                case EVERY_SEC: mListSecond.push_back(cb); break;
                case EVERY_MIN: mListMinute.push_back(cb); break;
                case EVERY_HR:  mListHour.push_back(cb); break;
                case EVERY_12H: mList12h.push_back(cb); break;
                case EVERY_DAY: mListDay.push_back(cb); break;
                default: break;
            }
        }

        uint32_t getUptime(void) {
            return mUptime;
        }

        uint32_t getTimestamp(void) {
            return mTimestamp;
        }

    protected:
        virtual void notify(std::list<SchedulerCb> *lType) {
            for(std::list<SchedulerCb>::iterator it = lType->begin(); it != lType->end(); ++it) {
               (*it)();
            }
        }

        uint32_t mTimestamp;

    private:
         void onceFuncTick(void) {
            if(mOnce.empty())
                return;
            for(std::list<once_t>::iterator it = mOnce.begin(); it != mOnce.end();) {
                if(((*it).n)-- == 0) {
                    ((*it).f)();
                    it = mOnce.erase(it);
                }
                else
                    ++it;
            }
        }

        void onceAtFuncTick(void) {
            if(mOnceAt.empty())
                return;
            for(std::list<once_t>::iterator it = mOnceAt.begin(); it != mOnceAt.end();) {
                if(((*it).n) < mTimestamp) {
                    ((*it).f)();
                    it = mOnceAt.erase(it);
                }
                else
                    ++it;
            }
        }

        std::list<SchedulerCb> mListSecond;
        std::list<SchedulerCb> mListMinute;
        std::list<SchedulerCb> mListHour;
        std::list<SchedulerCb> mList12h;
        std::list<SchedulerCb> mListDay;

        std::list<once_t> mOnce;
        std::list<once_t> mOnceAt;

        uint32_t mPrevMillis, mUptime;
        uint8_t mSeconds, mMinutes, mHours;
};
}

#endif /*__SCHEDULER_H__*/
