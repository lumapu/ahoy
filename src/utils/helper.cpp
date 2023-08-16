//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "helper.h"
#include "dbg.h"

namespace ah {
    void ip2Arr(uint8_t ip[], const char *ipStr) {
        uint8_t p = 1;
        memset(ip, 0, 4);
        for(uint8_t i = 0; i < 16; i++) {
            if(ipStr[i] == 0)
                return;
            if(0 == i)
                ip[0] = atoi(ipStr);
            else if(ipStr[i] == '.')
                ip[p++] = atoi(&ipStr[i+1]);
        }
    }

    // note: char *str needs to be at least 16 bytes long
    void ip2Char(uint8_t ip[], char *str) {
        if(0 == ip[0])
            str[0] = '\0';
        else
            snprintf(str, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    }

    double round3(double value) {
        return (int)(value * 1000 + 0.5) / 1000.0;
    }

    String getDateTimeStr(time_t t) {
        char str[20];
        if(0 == t)
            sprintf(str, "n/a");
        else
            sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", year(t), month(t), day(t), hour(t), minute(t), second(t));
        return String(str);
    }

    String getDateTimeStrFile(time_t t) {
        char str[20];
        if(0 == t)
            sprintf(str, "na");
        else
            sprintf(str, "%04d-%02d-%02d_%02d-%02d-%02d", year(t), month(t), day(t), hour(t), minute(t), second(t));
        return String(str);
    }

    String getDateTimeStrShort(time_t t) {
        char str[20];
        if(0 == t)
            sprintf(str, "n/a");
        else {
            sprintf(str, "%3s ", dayShortStr(dayOfWeek(t)));
            sprintf(str+4, "%2d.%3s %02d:%02d", day(t), monthShortStr(month(t)), hour(t), minute(t));
        }
        return String(str);
    }

    String getTimeStr(time_t t) {
        char str[9];
        if(0 == t)
            sprintf(str, "n/a");
        else
            sprintf(str, "%02d:%02d:%02d", hour(t), minute(t), second(t));
        return String(str);
    }

    uint64_t Serial2u64(const char *val) {
        char tmp[3];
        uint64_t ret = 0ULL;
        uint64_t u64;
        memset(tmp, 0, 3);
        for(uint8_t i = 0; i < 6; i++) {
            tmp[0] = val[i*2];
            tmp[1] = val[i*2 + 1];
            if((tmp[0] == '\0') || (tmp[1] == '\0'))
                break;
            u64 = strtol(tmp, NULL, 16);
            ret |= (u64 << ((5-i) << 3));
        }
        return ret;
    }

    void dumpBuf(uint8_t buf[], uint8_t len) {
        for(uint8_t i = 0; i < len; i++) {
            DHEX(buf[i]);
            DBGPRINT(" ");
        }
        DBGPRINTLN("");
    }
}
