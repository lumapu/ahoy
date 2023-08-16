//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HELPER_H__
#define __HELPER_H__

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <Timezone.h>

static TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120}; // Central European Summer Time
static TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};   // Central European Standard Time
static Timezone gTimezone(CEST, CET);


#define CHECK_MASK(a,b) ((a & b) == b)

#define CP_U32_LittleEndian(buf, v) ({ \
    uint8_t *b = buf; \
    b[0] = ((v >> 24) & 0xff); \
    b[1] = ((v >> 16) & 0xff); \
    b[2] = ((v >>  8) & 0xff); \
    b[3] = ((v      ) & 0xff); \
})

#define CP_U32_BigEndian(buf, v) ({ \
    uint8_t *b = buf; \
    b[3] = ((v >> 24) & 0xff); \
    b[2] = ((v >> 16) & 0xff); \
    b[1] = ((v >>  8) & 0xff); \
    b[0] = ((v      ) & 0xff); \
})

namespace ah {
    void ip2Arr(uint8_t ip[], const char *ipStr);
    void ip2Char(uint8_t ip[], char *str);
    double round3(double value);
    String getDateTimeStr(time_t t);
    String getDateTimeStrShort(time_t t);
    String getDateTimeStrFile(time_t t);
    String getTimeStr(time_t t);
    uint64_t Serial2u64(const char *val);
    void dumpBuf(uint8_t buf[], uint8_t len);
}

#endif /*__HELPER_H__*/
