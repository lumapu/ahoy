//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------
#include <Arduino.h>
#include "helper.h"
#include "dbg.h"
#include "../plugins/plugin_lang.h"

#define dt_SHORT_STR_LEN_i18n  3 // the length of short strings
static char buffer_i18n[dt_SHORT_STR_LEN_i18n + 1];  // must be big enough for longest string and the terminating null
const char monthShortNames_P[] PROGMEM = STR_MONTHNAME_3_CHAR_LIST;
const char dayShortNames_P[] PROGMEM = STR_DAYNAME_3_CHAR_LIST;

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

    double round1(double value) {
        return (int)(value * 10 + 0.5) / 10.0;
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

    String getTimeStrMs(uint64_t t) {
        char str[13];
        if(0 == t)
            sprintf(str, "n/a");
        else {
            uint16_t m = t % 1000;
            t = t / 1000;
            sprintf(str, "%02d:%02d:%02d.%03d", hour(t), minute(t), second(t), m);
        }
        return String(str);
    }

    static char* monthShortStr_i18n(uint8_t month) {
        for (int i=0; i < dt_SHORT_STR_LEN_i18n; i++)
            buffer_i18n[i] = pgm_read_byte(&(monthShortNames_P[i + month * dt_SHORT_STR_LEN_i18n]));
        buffer_i18n[dt_SHORT_STR_LEN_i18n] = 0;
        return buffer_i18n;
    }

    static char* dayShortStr_i18n(uint8_t day) {
        for (int i=0; i < dt_SHORT_STR_LEN_i18n; i++)
            buffer_i18n[i] = pgm_read_byte(&(dayShortNames_P[i + day * dt_SHORT_STR_LEN_i18n]));
        buffer_i18n[dt_SHORT_STR_LEN_i18n] = 0;
        return buffer_i18n;
    }

    String getDateTimeStrShort_i18n(time_t t) {
        char str[20];
        if(0 == t)
            sprintf(str, "n/a");
        else {
            sprintf(str, "%3s ", dayShortStr_i18n(dayOfWeek(t)));
            sprintf(str+4, "%2d.%3s %02d:%02d", day(t), monthShortStr_i18n(month(t)), hour(t), minute(t));
        }
        return String(str);
    }

    uint64_t Serial2u64(const char *val) {
        char tmp[3];
        uint64_t ret = 0ULL;
        memset(tmp, 0, 3);
        for(uint8_t i = 0; i < 6; i++) {
            tmp[0] = val[i*2];
            tmp[1] = val[i*2 + 1];
            if((tmp[0] == '\0') || (tmp[1] == '\0'))
                break;
            uint64_t u64 = strtol(tmp, NULL, 16);
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

    float readTemperature() {
        /*// ADC1 channel 0 is GPIO36
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
        int adc_reading = adc1_get_raw(ADC1_CHANNEL_0);
        // Convert the raw ADC reading to a voltage in mV
        esp_adc_cal_characteristics_t characteristics;
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, 1100, &characteristics);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &characteristics);
        // Convert the voltage to a temperature in Celsius
        // This formula is an approximation and might need to be calibrated for your specific use case.
        float temperature = (voltage - 500) / 10.0;*/

        #if defined(ESP32)
        return temperatureRead();
        #else
        return 0;
        #endif
    }
}
