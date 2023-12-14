//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __COMM_QUEUE_H__
#define __COMM_QUEUE_H__

#include <array>
#include <functional>
#include "hmInverter.h"
#include "../utils/dbg.h"

template <uint8_t N=100>
class CommQueue {
    public:
        CommQueue() {}

        void addImportant(Inverter<> *iv, uint8_t cmd, bool delOnPop = true) {
            dec(&mRdPtr);
            mQueue[mRdPtr] = queue_s(iv, cmd, delOnPop, true);
        }

        void add(Inverter<> *iv, uint8_t cmd, bool delOnPop = true) {
            mQueue[mWrPtr] = queue_s(iv, cmd, delOnPop, false);
            inc(&mWrPtr);
        }

        void chgCmd(Inverter<> *iv, uint8_t cmd, bool delOnPop = true) {
            mQueue[mWrPtr] = queue_s(iv, cmd, delOnPop, false);
        }

        uint8_t getFillState(void) {
            return abs(mRdPtr - mWrPtr);
        }

        uint8_t getMaxFill(void) {
            return N;
        }

    protected:
        struct queue_s {
            Inverter<> *iv;
            uint8_t cmd;
            uint8_t attempts;
            uint32_t ts;
            bool delOnPop;
            bool isDevControl;
            queue_s() {}
            queue_s(Inverter<> *i, uint8_t c, bool d, bool dev) :
                iv(i), cmd(c), attempts(5), ts(0), delOnPop(d), isDevControl(dev) {}
        };

    protected:
        void add(queue_s q) {
            mQueue[mWrPtr] = q;
            inc(&mWrPtr);
        }

        void add(const queue_s *q, bool rstAttempts = false) {
            mQueue[mWrPtr] = *q;
            if(rstAttempts)
                mQueue[mWrPtr].attempts = 5;
            inc(&mWrPtr);
        }

        void chgCmd(uint8_t cmd) {
            mQueue[mRdPtr].cmd = cmd;
            mQueue[mRdPtr].isDevControl = false;
        }

        void get(std::function<void(bool valid, const queue_s *q)> cb) {
            if(mRdPtr == mWrPtr) {
                cb(false, &mQueue[mRdPtr]); // empty
                return;
            }
            cb(true, &mQueue[mRdPtr]);
        }

        void cmdDone(bool force = false) {
            if(!mQueue[mRdPtr].delOnPop && !force) {
                mQueue[mRdPtr].attempts = 5;
                add(mQueue[mRdPtr]); // add to the end again
            }
            inc(&mRdPtr);
        }

        void setTs(uint32_t *ts) {
            mQueue[mRdPtr].ts = *ts;
        }

        void setAttempt(void) {
            if(mQueue[mRdPtr].attempts)
                mQueue[mRdPtr].attempts--;
        }

        void incrAttempt(void) {
            mQueue[mRdPtr].attempts++;
        }

        void inc(uint8_t *ptr) {
            if(++(*ptr) >= N)
                *ptr = 0;
        }
        void dec(uint8_t *ptr) {
            if((*ptr) == 0)
                *ptr = N-1;
            else
                --(*ptr);
        }

    protected:
        std::array<queue_s, N> mQueue;
        uint8_t mWrPtr = 0;
        uint8_t mRdPtr = 0;
};


#endif /*__COMM_QUEUE_H__*/
