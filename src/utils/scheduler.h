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

namespace ah {
class Scheduler {
    public:
        Scheduler() {}

        void setup() {
            mPrevMillis = 0;
            mSeconds = 0;
            mMinutes = 0;
            mHours = 0;
        }

        void loop() {
            if (millis() - mPrevMillis >= 1000) {
                mPrevMillis += 1000;
                notify(&mListSecond);
                if(++mSeconds >= 60) {
                    mSeconds = 0;
                    notify(&mListMinute);
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

        virtual void notify(std::list<SchedulerCb> *lType) {
            for(std::list<SchedulerCb>::iterator it = lType->begin(); it != lType->end(); ++it) {
               (*it)();
            }
        }

    protected:
        std::list<SchedulerCb> mListSecond;
        std::list<SchedulerCb> mListMinute;
        std::list<SchedulerCb> mListHour;
        std::list<SchedulerCb> mList12h;
        std::list<SchedulerCb> mListDay;

    private:
        uint32_t mPrevMillis;
        uint8_t mSeconds, mMinutes, mHours;
};
}

#endif /*__SCHEDULER_H__*/
