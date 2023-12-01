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

    #define dt_SHORT_STR_LEN_i18n  3 // the length of short strings
    static char buffer[dt_SHORT_STR_LEN_i18n + 1];  // must be big enough for longest string and the terminating null
    const char monthShortNames_ge_P[] PROGMEM = "ErrJanFebMrzAprMaiJunJulAugSepOktNovDez";
    const char dayShortNames_ge_P[] PROGMEM = "ErrSonMonDi.Mi.Do.Fr.Sam";
    const char monthShortNames_fr_P[] PROGMEM = "ErrJanFevMarAvrMaiJunJulAouSepOctNovDec";
    const char dayShortNames_fr_P[] PROGMEM = "ErrDimLunMarMerJeuVenSam";

    char* monthShortStr_i18n(uint8_t month, enum lang language)
    {
        const char *monthShortNames_P;

        switch (language) {
            case LANG_EN:
                return(monthShortStr(month));
            case LANG_GE:
                monthShortNames_P =monthShortNames_ge_P;
                break;
            case LANG_FR:
                monthShortNames_P =monthShortNames_fr_P;
                break;
        }
        for (int i=0; i < dt_SHORT_STR_LEN_i18n; i++)
            buffer[i] = pgm_read_byte(&(monthShortNames_P[i + month * dt_SHORT_STR_LEN_i18n]));
        buffer[dt_SHORT_STR_LEN_i18n] = 0;
        return buffer;
    }

    char* dayShortStr_i18n(uint8_t day, enum lang language)
    {
        const char *dayShortNames_P;

        switch (language) {
            case LANG_EN:
                return(dayShortStr(day));
            case LANG_GE:
                dayShortNames_P = dayShortNames_ge_P;
                break;
            case LANG_FR:
                dayShortNames_P = dayShortNames_fr_P;
                break;
        }
        for (int i=0; i < dt_SHORT_STR_LEN_i18n; i++)
            buffer[i] = pgm_read_byte(&(dayShortNames_P[i + day * dt_SHORT_STR_LEN_i18n]));
        buffer[dt_SHORT_STR_LEN_i18n] = 0;
        return buffer;
    }

    String getDateTimeStrShort_i18n(time_t t, enum lang language) {
        char str[20];
        if(0 == t)
            sprintf(str, "n/a");
        else {
            sprintf(str, "%3s ", dayShortStr_i18n(dayOfWeek(t), language));
            sprintf(str+4, "%2d.%3s %02d:%02d", day(t), monthShortStr_i18n(month(t), language), hour(t), minute(t));
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

    String getTimeStrMs(time_t t) {
        char str[13];
        if(0 == t)
            sprintf(str, "n/a");
        else
            sprintf(str, "%02d:%02d:%02d.%03d", hour(t), minute(t), second(t), millis() % 1000);
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

    void dumpBuf(uint8_t buf[], uint8_t len, uint8_t firstRepl, uint8_t lastRepl) {
        for(uint8_t i = 0; i < len; i++) {
            if((i < firstRepl) || (i > lastRepl) || (0 == firstRepl))
                DHEX(buf[i]);
            else
                DBGPRINT(F(" *"));
            DBGPRINT(" ");
        }
        DBGPRINTLN("");
    }
}
