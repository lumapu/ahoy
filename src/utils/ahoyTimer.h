//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __AHOY_TIMER_H__
#define __AHOY_TIMER_H__

#include <Arduino.h>

namespace ah {
    inline bool checkTicker(uint32_t *ticker, uint32_t interval) {
        uint32_t mil = millis();
        if(mil >= *ticker) {
            *ticker = mil + interval;
            return true;
        }
        else if((mil + interval) < (*ticker)) {
            *ticker = mil + interval;
            return true;
        }

        return false;
    }
}

#endif /*__AHOY_TIMER_H__*/
