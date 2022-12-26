//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HELPER_H__
#define __HELPER_H__

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <TimeLib.h>


#define CHECK_MASK(a,b) ((a & b) == b)

namespace ah {
    void ip2Arr(uint8_t ip[], const char *ipStr);
    void ip2Char(uint8_t ip[], char *str);
    double round3(double value);
    String getDateTimeStr(time_t t);
    uint64_t Serial2u64(const char *val);
}

#endif /*__HELPER_H__*/
