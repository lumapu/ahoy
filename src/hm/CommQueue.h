//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __COMM_QUEUE_H__
#define __COMM_QUEUE_H__

#include <array>
#include <functional>
#include "hmInverter.h"

template <uint8_t N=100>
class CommQueue {
    public:
        CommQueue() {}

        void addImportant(Inverter<> *iv, uint8_t cmd, bool delOnPop = true) {
            dec(&mRdPtr);
            mQueue[mRdPtr] = queue_s(iv, cmd, delOnPop);
        }

        void add(Inverter<> *iv, uint8_t cmd, bool delOnPop = true) {
            mQueue[mWrPtr] = queue_s(iv, cmd, delOnPop);
            inc(&mWrPtr);
        }

    protected:
        struct queue_s {
            Inverter<> *iv;
            uint8_t cmd;
            uint8_t attempts;
            bool delOnPop;
            uint32_t ts;
            queue_s() {}
            queue_s(Inverter<> *i, uint8_t c, bool d) :
                iv(i), cmd(c), attempts(5), ts(0), delOnPop(d) {}
        };

    protected:
        void add(queue_s q) {
            mQueue[mWrPtr] = q;
            inc(&mWrPtr);
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
