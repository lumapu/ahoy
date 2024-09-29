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

#define DEFAULT_ATTEMPS                 5
#define MORE_ATTEMPS_ALARMDATA          3 // 8
#define MORE_ATTEMPS_GRIDONPROFILEPARA  0 // 5

template <uint8_t N=100>
class CommQueue {
    public:
        CommQueue()
            : wrPtr {0}
            , rdPtr {0}
        {
            this->mutex = xSemaphoreCreateBinaryStatic(&this->mutex_buffer);
            xSemaphoreGive(this->mutex);
        }

        ~CommQueue() {
            vSemaphoreDelete(this->mutex);
        }

        void addImportant(Inverter<> *iv, uint8_t cmd) {
            queue_s q(iv, cmd, true);
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(!isIncluded(&q)) {
                dec(&this->rdPtr);
                mQueue[this->rdPtr] = q;
            }
            xSemaphoreGive(this->mutex);
        }

        void add(Inverter<> *iv, uint8_t cmd) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            queue_s q(iv, cmd, false);
            if(!isIncluded(&q)) {
                mQueue[this->wrPtr] = q;
                inc(&this->wrPtr);
            }
            xSemaphoreGive(this->mutex);
        }

        void chgCmd(Inverter<> *iv, uint8_t cmd) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            mQueue[this->wrPtr] = queue_s(iv, cmd, false);
            xSemaphoreGive(this->mutex);
        }

        uint8_t getFillState(void) const {
            //DPRINTLN(DBG_INFO, "wr: " + String(this->wrPtr) + ", rd: " + String(this->rdPtr));
            return abs(this->rdPtr - this->wrPtr);
        }

        uint8_t getMaxFill(void) const {
            return N;
        }

    protected:
        struct queue_s {
            Inverter<> *iv;
            uint8_t cmd;
            uint8_t attempts;
            uint8_t attemptsMax;
            uint32_t ts;
            bool isDevControl;

            queue_s() {}

            queue_s(Inverter<> *i, uint8_t c, bool dev)
                : iv {i}
                , cmd {c}
                , attempts {DEFAULT_ATTEMPS}
                , attemptsMax {DEFAULT_ATTEMPS}
                , ts {0}
                , isDevControl {dev}
            {}

            queue_s(const queue_s &other) // copy constructor
                : iv {other.iv}
                , cmd {other.cmd}
                , attempts {other.attempts}
                , attemptsMax {other.attemptsMax}
                , ts {other.ts}
                , isDevControl {other.isDevControl}
            {}
        };

    protected:
        void add(queue_s q) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            mQueue[this->wrPtr] = q;
            inc(&this->wrPtr);
            xSemaphoreGive(this->mutex);
        }

        void add(queue_s *q, bool rstAttempts = false) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            mQueue[this->wrPtr] = *q;
            if(rstAttempts) {
                mQueue[this->wrPtr].attempts = DEFAULT_ATTEMPS;
                mQueue[this->wrPtr].attemptsMax = DEFAULT_ATTEMPS;
            }
            inc(&this->wrPtr);
            xSemaphoreGive(this->mutex);
        }

        void chgCmd(queue_s *q, uint8_t cmd) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            q->cmd = cmd;
            q->isDevControl = false;
            xSemaphoreGive(this->mutex);
        }

        void get(std::function<void(bool valid, queue_s *q)> cb) {
            if(this->rdPtr == this->wrPtr)
                cb(false, nullptr); // empty
            else {
                //xSemaphoreTake(this->mutex, portMAX_DELAY);
                //uint8_t tmp = this->rdPtr;
                //xSemaphoreGive(this->mutex);
                cb(true, &mQueue[this->rdPtr]);
            }
        }

        void cmdDone(queue_s *q, bool keep = false) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(keep) {
                q->attempts = DEFAULT_ATTEMPS;
                q->attemptsMax = DEFAULT_ATTEMPS;
                xSemaphoreGive(this->mutex);
                add(q); // add to the end again
                xSemaphoreTake(this->mutex, portMAX_DELAY);
            }
            inc(&this->rdPtr);
            xSemaphoreGive(this->mutex);
        }

        void setTs(queue_s *q, const uint32_t *ts) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            q->ts = *ts;
            xSemaphoreGive(this->mutex);
        }

        void setAttempt(queue_s *q) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(q->attempts)
                q->attempts--;
            xSemaphoreGive(this->mutex);
        }

        void incrAttempt(queue_s *q, uint8_t attempts = 1) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            q->attempts += attempts;
            if (q->attempts > q->attemptsMax)
                q->attemptsMax = q->attempts;
            xSemaphoreGive(this->mutex);
        }

    private:
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

        bool isIncluded(const queue_s *q) {
            uint8_t ptr = this->rdPtr;
            while (ptr != this->wrPtr) {
                if(mQueue[ptr].cmd == q->cmd) {
                    if(mQueue[ptr].iv->id == q->iv->id) {
                        if(mQueue[ptr].isDevControl == q->isDevControl)
                            return true;
                    }
                }
                inc(&ptr);
            }
            return false;
        }

    protected:
        std::array<queue_s, N> mQueue;

    private:
        uint8_t wrPtr;
        uint8_t rdPtr;
        SemaphoreHandle_t mutex;
        StaticSemaphore_t mutex_buffer;
};


#endif /*__COMM_QUEUE_H__*/
