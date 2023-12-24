//-----------------------------------------------------------
// You69Man, 2023
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// version 2 as published by the Free Software Foundation.
//-----------------------------------------------------------

/**
 * @file timemonitor.h
 *
 * Class declaration for TimeMonitor
 */

#ifndef __TIMEMONITOR_H__
#define __TIMEMONITOR_H__

#include <Arduino.h>

class TimeMonitor {
    public:
        /**
         * A constructor for initializing a TimeMonitor
         * @note TimeMonitor witch default constructor is stopped
         */
        TimeMonitor(void) {};

        /**
         * A constructor for initializing a TimeMonitor
         * @param timeout timeout in ms
         * @param start   (optional) if true, start TimeMonitor immediately
         * @note TimeMonitor witch default constructor is stopped
         */
        TimeMonitor(uint32_t timeout, bool start = false) {
            if (start)
                startTimeMonitor(timeout);
            else
                configureTimeMonitor(timeout);
        };

        /**
         * Start the TimeMonitor with new timeout configuration
         * @param timeout timout in ms
         */
        void startTimeMonitor(uint32_t timeout) {
            mStartTime = millis();
            mTimeout = timeout;
            mStarted = true;
        }

        /**
         * Restart the TimeMonitor with already set timeout configuration
         * @note returns nothing
         */
        void reStartTimeMonitor(void) {
            mStartTime = millis();
            mStarted = true;
        }

        /**
         * Configure the TimeMonitor to new timeout configuration
         * @param timeout timeout in ms
         * @note This doesn't restart an already running TimeMonitor.
         *       If timer is already running and new timeout is longer that current timeout runtime of the running timer is expanded
         *       If timer is already running and new timeout is shorter that current timeout this can immediately lead to a timeout
         */
        void configureTimeMonitor(uint32_t timeout) {
            mTimeout = timeout;
        }

        /**
         * Stop the TimeMonitor
         */
        void stopTimeMonitor(void) {
            mStarted = false;
        }

        /**
         * Get timeout status of the TimeMonitor
         * @return bool
         *         true:  TimeMonitor already timed out
         *         false: TimeMonitor still in time or TimeMonitor was stopped
         */
        bool isTimeout(void) {
            if ((mStarted) && (millis() - mStartTime >= mTimeout))
                return true;
            else
                return false;
        }

        /**
         * Get timeout configuration of the TimeMonitor
         * @return uint32_t timeout value in ms
         */
        uint32_t getTimeout(void) const {
            return mTimeout;
        }

        /**
         * Get residual time of the TimeMonitor until timeout
         * @return uint32_t residual time until timeout in ms
         * @note   in case of a stopped TimeMonitor residual time is always 0xFFFFFFFFUL
         *         in case of a timed out TimeMonitor residual time is always 0UL (zero)
         */
        uint32_t getResidualTime(void) const {
            uint32_t delayed =  millis() - mStartTime;
            return(mStarted ? (delayed < mTimeout ? mTimeout - delayed : 0UL) : 0xFFFFFFFFUL);
        }

        /**
         * Get overall run time of the TimeMonitor
         * @return uint32_t residual time until timeout in ms
         * @note   in case of a stopped TimeMonitor residual time is always 0xFFFFFFFFUL
         *         in case of a timed out TimeMonitor residual time is always 0UL (zero)
         */
        uint32_t getRunTime(void) const {
            return(mStarted ? millis() - mStartTime : 0UL);
        }

    private:
        uint32_t mStartTime = 0UL;  // start time of the TimeMonitor
        uint32_t mTimeout = 0UL;    // timeout configuration of the TimeMonitor
        bool     mStarted = false;  // start/stop state of the TimeMonitor
};

#endif