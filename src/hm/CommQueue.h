//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __COMM_QUEUE_H__
#define __COMM_QUEUE_H__

#include <array>
#include <functional>
#include "hmInverter.h"
#include "../utils/dbg.h"

#if !defined(ESP32)
    #if !defined(vSemaphoreDelete)
        #define vSemaphoreDelete(a)
        #define xSemaphoreTake(a, b) { while(a) { yield(); } a = true; }
        #define xSemaphoreGive(a) { a = false; }
    #endif
#endif

template <uint8_t N=100>
class CommQueue {
    protected: /* types */
        static constexpr uint8_t DefaultAttempts = 5;
        static constexpr uint8_t MoreAttemptsAlarmData = 3;
        static constexpr uint8_t MoreAttemptsGridProfile = 0;

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

            QueueElement(const QueueElement&) = delete;

            QueueElement(QueueElement&& other) : QueueElement{} {
                this->swap(other);
            }

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

            QueueElement& operator=(const QueueElement&) = delete;

            QueueElement& operator = (QueueElement&& other) {
                this->swap(other);
                return *this;
            }

            void swap(QueueElement& other) {
                std::swap(this->iv, other.iv);
                std::swap(this->cmd, other.cmd);
                std::swap(this->attempts, other.attempts);
                std::swap(this->attemptsMax, other.attemptsMax);
                std::swap(this->ts, other.ts);
                std::swap(this->isDevControl, other.isDevControl);
            }
        };

    public:
        CommQueue()
            : wrPtr {0}
            , rdPtr {0}
        {
            #if defined(ESP32)
            this->mutex = xSemaphoreCreateBinaryStatic(&this->mutex_buffer);
            xSemaphoreGive(this->mutex);
            #endif
        }

        ~CommQueue() {
            vSemaphoreDelete(this->mutex);
        }

        void addImportant(Inverter<> *iv, uint8_t cmd) {
            QueueElement q(iv, cmd, true);
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(!isIncluded(&q)) {
                dec(&this->rdPtr);
                mQueue[this->rdPtr] = std::move(q);
            }
            xSemaphoreGive(this->mutex);
        }

        void add(Inverter<> *iv, uint8_t cmd) {
            QueueElement q(iv, cmd, false);
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(!isIncluded(&q)) {
                mQueue[this->wrPtr] = std::move(q);
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
        void add(QueueElement q) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            mQueue[this->wrPtr] = q;
            inc(&this->wrPtr);
            xSemaphoreGive(this->mutex);
        }

        void add(QueueElement *q, bool rstAttempts = false) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(rstAttempts) {
                q->attempts = DefaultAttempts;
                q->attemptsMax = DefaultAttempts;
            }
            mQueue[this->wrPtr] = std::move(*q);
            inc(&this->wrPtr);
            xSemaphoreGive(this->mutex);
        }

        void get(std::function<void(bool valid, QueueElement *q)> cb) {
            xSemaphoreTake(this->mutex, portMAX_DELAY);
            if(this->rdPtr == this->wrPtr) {
                xSemaphoreGive(this->mutex);
                cb(false, nullptr); // empty
            } else {
                QueueElement el = std::move(mQueue[this->rdPtr]);
                inc(&this->rdPtr);
                xSemaphoreGive(this->mutex);
                cb(true, &el);
            }
        }

        void cmdReset(QueueElement *q) {
            q->attempts = DefaultAttempts;
            q->attemptsMax = DefaultAttempts;
            add(q); // add to the end again
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
        #if defined(ESP32)
        SemaphoreHandle_t mutex;
        StaticSemaphore_t mutex_buffer;
        #else
        bool mutex;
        #endif
};


#endif /*__COMM_QUEUE_H__*/
