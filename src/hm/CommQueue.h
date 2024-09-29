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
    protected: /* types */
        static constexpr uint8_t DefaultAttempts = 5;
        static constexpr uint8_t MoreAttemptsAlarmData = 3;
        static constexpr uint8_t MoreAttemptsGridProfile = 0;

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
            QueueElement q(iv, cmd, true);
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(!isIncluded(&q)) {
                dec(&this->rdPtr);
                mQueue[this->rdPtr] = q;
            }
            xSemaphoreGive(this->mutex);
        }

        void add(Inverter<> *iv, uint8_t cmd) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            QueueElement q(iv, cmd, false);
            if(!isIncluded(&q)) {
                mQueue[this->wrPtr] = q;
                inc(&this->wrPtr);
            }
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
        struct QueueElement {
            Inverter<> *iv;
            uint8_t cmd;
            uint8_t attempts;
            uint8_t attemptsMax;
            uint32_t ts;
            bool isDevControl;

            QueueElement()
                : iv {nullptr}
                , cmd {0}
                , attempts {0}
                , attemptsMax {0}
                , ts {0}
                , isDevControl {false}
            {}

            QueueElement(Inverter<> *iv, uint8_t cmd, bool devCtrl)
                : iv {iv}
                , cmd {cmd}
                , attempts {DefaultAttempts}
                , attemptsMax {DefaultAttempts}
                , ts {0}
                , isDevControl {devCtrl}
            {}

            QueueElement(const QueueElement &other) // copy constructor
                : iv {other.iv}
                , cmd {other.cmd}
                , attempts {other.attempts}
                , attemptsMax {other.attemptsMax}
                , ts {other.ts}
                , isDevControl {other.isDevControl}
            {}

            void changeCmd(uint8_t cmd) {
                this->cmd = cmd;
                this->isDevControl = false;
            }

            void setTs(const uint32_t ts) {
                this->ts = ts;
            }

            void setAttempt() {
                if(this->attempts)
                    this->attempts--;
            }

            void incrAttempt(uint8_t attempts = 1) {
                this->attempts += attempts;
                if (this->attempts > this->attemptsMax)
                    this->attemptsMax = this->attempts;
            }
        };

    protected:
        void add(QueueElement q) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            mQueue[this->wrPtr] = q;
            inc(&this->wrPtr);
            xSemaphoreGive(this->mutex);
        }

        void add(QueueElement *q, bool rstAttempts = false) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            mQueue[this->wrPtr] = *q;
            if(rstAttempts) {
                mQueue[this->wrPtr].attempts = DefaultAttempts;
                mQueue[this->wrPtr].attemptsMax = DefaultAttempts;
            }
            inc(&this->wrPtr);
            xSemaphoreGive(this->mutex);
        }

        void get(std::function<void(bool valid, QueueElement *q)> cb) {
            if(this->rdPtr == this->wrPtr)
                cb(false, nullptr); // empty
            else {
                //xSemaphoreTake(this->mutex, portMAX_DELAY);
                //uint8_t tmp = this->rdPtr;
                //xSemaphoreGive(this->mutex);
                cb(true, &mQueue[this->rdPtr]);
            }
        }

        void cmdDone(QueueElement *q, bool keep = false) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(keep) {
                q->attempts = DefaultAttempts;
                q->attemptsMax = DefaultAttempts;
                xSemaphoreGive(this->mutex);
                add(q); // add to the end again
                xSemaphoreTake(this->mutex, portMAX_DELAY);
            }
            inc(&this->rdPtr);
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

        bool isIncluded(const QueueElement *q) {
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
        std::array<QueueElement, N> mQueue;

    private:
        uint8_t wrPtr;
        uint8_t rdPtr;
        SemaphoreHandle_t mutex;
        StaticSemaphore_t mutex_buffer;
};


#endif /*__COMM_QUEUE_H__*/
