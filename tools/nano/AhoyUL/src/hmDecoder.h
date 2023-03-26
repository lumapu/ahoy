//-----------------------------------------------------------------------------
// 2022 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

// 2022 mb using PROGMEM for lage char* arrays, used and tested for HM800 decoding (2channel), no calculation yet
//   - strip down of hmDefines.h and rename to hmDecode.h

// todo: make a decoding class out of this

#ifndef __HM_DEFINES_H__
#define __HM_DEFINES_H__

#include "dbg.h"

union serial_u {
    uint64_t u64;
    uint8_t b[8];
};

// units
enum { UNIT_V = 0,
       UNIT_A,
       UNIT_W,
       UNIT_WH,
       UNIT_KWH,
       UNIT_HZ,
       UNIT_C,
       UNIT_PCT,
       UNIT_VAR,
       UNIT_NONE };
// const char* const units[] = {" V", " A", " W", " Wh", " kWh", " Hz", " °C", " %", " var", ""};
const char PGM_units[][6] PROGMEM = {" V ", " A ", " W ", " Wh ", " kWh ", " Hz ", " °C ", " % ", " var ", " "};  // one hidden byte needed at the end for '\0'

// field types
enum { FLD_UDC = 0,
       FLD_IDC,
       FLD_PDC,
       FLD_YD,
       FLD_YW,
       FLD_YT,
       FLD_UAC,
       FLD_IAC,
       FLD_PAC,
       FLD_F,
       FLD_T,
       FLD_PF,
       FLD_EFF,
       FLD_IRR,
       FLD_Q,
       FLD_EVT,
       FLD_FW_VERSION,
       FLD_FW_BUILD_YEAR,
       FLD_FW_BUILD_MONTH_DAY,
       FLD_FW_BUILD_HOUR_MINUTE,
       FLD_HW_ID,
       FLD_ACT_ACTIVE_PWR_LIMIT,
       /*FLD_ACT_REACTIVE_PWR_LIMIT, FLD_ACT_PF,*/ FLD_LAST_ALARM_CODE };

// PGM Flash memory usage instead of RAM for ARDUINO NANO, idea given by Nick Gammon's great webpage http://gammon.com.au/progmem
const char PGM_fields[/*NUM ELEMENTS*/][25] PROGMEM = {
    {"U_DC"},
    {"I_DC"},
    {"P_DC"},
    {"YieldDay"},
    {"YieldWeek"},
    {"YieldTotal"},
    {"U_AC"},
    {"I_AC"},
    {"P_AC"},
    {"F_AC"},
    {"Temp"},
    {"PF_AC"},
    {"Efficiency"},
    {"Irradiation"},
    {"Q_AC"},
    {"ALARM_MES_ID"},
    {"FWVersion"},
    {"FWBuildYear"},
    {"FWBuildMonthDay"},
    {"FWBuildHourMinute"},
    {"HWPartId"},
    {"active PowerLimit"},
    /* {"reactive PowerLimit"},
    {"Powerfactor"}*/
    {"LastAlarmCode"},
};


const char PGM_invname[][8] PROGMEM = { "HM400", "HM600", "HM800", "HM1200", "HM1500"};

// const char* const fields[] = {"U_DC", "I_DC", "P_DC", "YieldDay", "YieldWeek", "YieldTotal",
//         "U_AC", "I_AC", "P_AC", "F_AC", "Temp", "PF_AC", "Efficiency", "Irradiation","Q_AC",
//         "ALARM_MES_ID","FWVersion","FWBuildYear","FWBuildMonthDay","FWBuildHourMinute","HWPartId",
//         "active PowerLimit", /*"reactive PowerLimit","Powerfactor",*/ "LastAlarmCode"};

const char* const notAvail = "n/a";

// mqtt discovery device classes
enum { DEVICE_CLS_NONE = 0,
       DEVICE_CLS_CURRENT,
       DEVICE_CLS_ENERGY,
       DEVICE_CLS_PWR,
       DEVICE_CLS_VOLTAGE,
       DEVICE_CLS_FREQ,
       DEVICE_CLS_TEMP };
const char* const deviceClasses[] = {0, "current", "energy", "power", "voltage", "frequency", "temperature"};
enum { STATE_CLS_NONE = 0,
       STATE_CLS_MEASUREMENT,
       STATE_CLS_TOTAL_INCREASING };
const char* const stateClasses[] = {0, "measurement", "total_increasing"};
typedef struct {
    uint8_t fieldId;      // field id
    uint8_t deviceClsId;  // device class
    uint8_t stateClsId;   // state class
} byteAssign_fieldDeviceClass;
const byteAssign_fieldDeviceClass deviceFieldAssignment[] = {
    {FLD_UDC, DEVICE_CLS_VOLTAGE, STATE_CLS_MEASUREMENT},
    {FLD_IDC, DEVICE_CLS_CURRENT, STATE_CLS_MEASUREMENT},
    {FLD_PDC, DEVICE_CLS_PWR, STATE_CLS_MEASUREMENT},
    {FLD_YD, DEVICE_CLS_ENERGY, STATE_CLS_TOTAL_INCREASING},
    {FLD_YW, DEVICE_CLS_ENERGY, STATE_CLS_TOTAL_INCREASING},
    {FLD_YT, DEVICE_CLS_ENERGY, STATE_CLS_TOTAL_INCREASING},
    {FLD_UAC, DEVICE_CLS_VOLTAGE, STATE_CLS_MEASUREMENT},
    {FLD_IAC, DEVICE_CLS_CURRENT, STATE_CLS_MEASUREMENT},
    {FLD_PAC, DEVICE_CLS_PWR, STATE_CLS_MEASUREMENT},
    {FLD_F, DEVICE_CLS_FREQ, STATE_CLS_NONE},
    {FLD_T, DEVICE_CLS_TEMP, STATE_CLS_MEASUREMENT},
    {FLD_PF, DEVICE_CLS_NONE, STATE_CLS_NONE},
    {FLD_EFF, DEVICE_CLS_NONE, STATE_CLS_NONE},
    {FLD_IRR, DEVICE_CLS_NONE, STATE_CLS_NONE}};
#define DEVICE_CLS_ASSIGN_LIST_LEN (sizeof(deviceFieldAssignment) / sizeof(byteAssign_fieldDeviceClass))

// indices to calculation functions, defined in hmInverter.h
enum { CALC_YT_CH0 = 0,
       CALC_YD_CH0,
       CALC_UDC_CH,
       CALC_PDC_CH0,
       CALC_EFF_CH0,
       CALC_IRR_CH };
enum { CMD_CALC = 0xffff };

// CH0 is default channel (freq, ac, temp)
enum { CH0 = 0,
       CH1,
       CH2,
       CH3,
       CH4 };

enum { INV_TYPE_1CH = 0,
       INV_TYPE_2CH,
       INV_TYPE_4CH };

typedef struct {
    uint8_t fieldId;  // field id
    uint8_t unitId;   // uint id
    uint8_t ch;       // channel 0 - 4
    uint8_t start;    // pos of first byte in buffer
    uint8_t num;      // number of bytes in buffer
    uint16_t div;     // divisor / calc command
} byteAssign_t;

/**
 *  indices are built for the buffer starting with cmd-id in first byte
 *  (complete payload in buffer)
 * */

//-------------------------------------
// HM-Series
//-------------------------------------
const byteAssign_t InfoAssignment[] = {
    {FLD_FW_VERSION, UNIT_NONE, CH0, 0, 2, 1},
    {FLD_FW_BUILD_YEAR, UNIT_NONE, CH0, 2, 2, 1},
    {FLD_FW_BUILD_MONTH_DAY, UNIT_NONE, CH0, 4, 2, 1},
    {FLD_FW_BUILD_HOUR_MINUTE, UNIT_NONE, CH0, 6, 2, 1},
    {FLD_HW_ID, UNIT_NONE, CH0, 8, 2, 1}};
#define HMINFO_LIST_LEN (sizeof(InfoAssignment) / sizeof(byteAssign_t))
#define HMINFO_PAYLOAD_LEN 14

const byteAssign_t SystemConfigParaAssignment[] = {
    {FLD_ACT_ACTIVE_PWR_LIMIT, UNIT_PCT, CH0, 2, 2, 10} /*,
{ FLD_ACT_REACTIVE_PWR_LIMIT,  UNIT_PCT,   CH0,  4, 2, 10   },
{ FLD_ACT_PF,                  UNIT_NONE,  CH0,  6, 2, 1000 }*/
};
#define HMSYSTEM_LIST_LEN (sizeof(SystemConfigParaAssignment) / sizeof(byteAssign_t))
#define HMSYSTEM_PAYLOAD_LEN 14

const byteAssign_t AlarmDataAssignment[] = {
    {FLD_LAST_ALARM_CODE, UNIT_NONE, CH0, 0, 2, 1}};
#define HMALARMDATA_LIST_LEN (sizeof(AlarmDataAssignment) / sizeof(byteAssign_t))
#define HMALARMDATA_PAYLOAD_LEN 0  // 0: means check is off

//-------------------------------------
// HM300, HM350, HM400
//-------------------------------------
const byteAssign_t hm1chAssignment[] = {
    {FLD_UDC, UNIT_V, CH1, 2, 2, 10},
    {FLD_IDC, UNIT_A, CH1, 4, 2, 100},
    {FLD_PDC, UNIT_W, CH1, 6, 2, 10},
    {FLD_YD, UNIT_WH, CH1, 12, 2, 1},
    {FLD_YT, UNIT_KWH, CH1, 8, 4, 1000},
    {FLD_IRR, UNIT_PCT, CH1, CALC_IRR_CH, CH1, CMD_CALC},

    {FLD_UAC, UNIT_V, CH0, 14, 2, 10},
    {FLD_IAC, UNIT_A, CH0, 22, 2, 100},
    {FLD_PAC, UNIT_W, CH0, 18, 2, 10},
    {FLD_Q, UNIT_VAR, CH0, 20, 2, 10},
    {FLD_F, UNIT_HZ, CH0, 16, 2, 100},
    {FLD_PF, UNIT_NONE, CH0, 24, 2, 1000},
    {FLD_T, UNIT_C, CH0, 26, 2, 10},
    {FLD_EVT, UNIT_NONE, CH0, 28, 2, 1},
    {FLD_YD, UNIT_WH, CH0, CALC_YD_CH0, 0, CMD_CALC},
    {FLD_YT, UNIT_KWH, CH0, CALC_YT_CH0, 0, CMD_CALC},
    {FLD_PDC, UNIT_W, CH0, CALC_PDC_CH0, 0, CMD_CALC},
    {FLD_EFF, UNIT_PCT, CH0, CALC_EFF_CH0, 0, CMD_CALC}};
#define HM1CH_LIST_LEN (sizeof(hm1chAssignment) / sizeof(byteAssign_t))
#define HM1CH_PAYLOAD_LEN 30

//-------------------------------------
// HM600, HM700, HM800
//-------------------------------------
const byteAssign_t hm2chAssignment[] = {
    {FLD_UDC, UNIT_V, CH1, 2, 2, 10},
    {FLD_IDC, UNIT_A, CH1, 4, 2, 100},
    {FLD_PDC, UNIT_W, CH1, 6, 2, 10},
    {FLD_YD, UNIT_WH, CH1, 22, 2, 1},
    {FLD_YT, UNIT_KWH, CH1, 14, 4, 1000},
    {FLD_IRR, UNIT_PCT, CH1, CALC_IRR_CH, CH1, CMD_CALC},

    {FLD_UDC, UNIT_V, CH2, 8, 2, 10},
    {FLD_IDC, UNIT_A, CH2, 10, 2, 100},
    {FLD_PDC, UNIT_W, CH2, 12, 2, 10},
    {FLD_YD, UNIT_WH, CH2, 24, 2, 1},
    {FLD_YT, UNIT_KWH, CH2, 18, 4, 1000},
    {FLD_IRR, UNIT_PCT, CH2, CALC_IRR_CH, CH2, CMD_CALC},

    {FLD_UAC, UNIT_V, CH0, 26, 2, 10},
    {FLD_IAC, UNIT_A, CH0, 34, 2, 100},
    {FLD_PAC, UNIT_W, CH0, 30, 2, 10},
    {FLD_Q, UNIT_VAR, CH0, 32, 2, 10},
    {FLD_F, UNIT_HZ, CH0, 28, 2, 100},
    {FLD_PF, UNIT_NONE, CH0, 36, 2, 1000},
    {FLD_T, UNIT_C, CH0, 38, 2, 10},
    {FLD_EVT, UNIT_NONE, CH0, 40, 2, 1},
    {FLD_YD, UNIT_WH, CH0, CALC_YD_CH0, 0, CMD_CALC},
    {FLD_YT, UNIT_KWH, CH0, CALC_YT_CH0, 0, CMD_CALC},
    {FLD_PDC, UNIT_W, CH0, CALC_PDC_CH0, 0, CMD_CALC},
    {FLD_EFF, UNIT_PCT, CH0, CALC_EFF_CH0, 0, CMD_CALC}

};
#define HM2CH_LIST_LEN (sizeof(hm2chAssignment) / sizeof(byteAssign_t))
#define HM2CH_PAYLOAD_LEN 42

//-------------------------------------
// HM1200, HM1500
//-------------------------------------
const byteAssign_t hm4chAssignment[] = {
    {FLD_UDC, UNIT_V, CH1, 2, 2, 10},
    {FLD_IDC, UNIT_A, CH1, 4, 2, 100},
    {FLD_PDC, UNIT_W, CH1, 8, 2, 10},
    {FLD_YD, UNIT_WH, CH1, 20, 2, 1},
    {FLD_YT, UNIT_KWH, CH1, 12, 4, 1000},
    {FLD_IRR, UNIT_PCT, CH1, CALC_IRR_CH, CH1, CMD_CALC},

    {FLD_UDC, UNIT_V, CH2, CALC_UDC_CH, CH1, CMD_CALC},
    {FLD_IDC, UNIT_A, CH2, 6, 2, 100},
    {FLD_PDC, UNIT_W, CH2, 10, 2, 10},
    {FLD_YD, UNIT_WH, CH2, 22, 2, 1},
    {FLD_YT, UNIT_KWH, CH2, 16, 4, 1000},
    {FLD_IRR, UNIT_PCT, CH2, CALC_IRR_CH, CH2, CMD_CALC},

    {FLD_UDC, UNIT_V, CH3, 24, 2, 10},
    {FLD_IDC, UNIT_A, CH3, 26, 2, 100},
    {FLD_PDC, UNIT_W, CH3, 30, 2, 10},
    {FLD_YD, UNIT_WH, CH3, 42, 2, 1},
    {FLD_YT, UNIT_KWH, CH3, 34, 4, 1000},
    {FLD_IRR, UNIT_PCT, CH3, CALC_IRR_CH, CH3, CMD_CALC},

    {FLD_UDC, UNIT_V, CH4, CALC_UDC_CH, CH3, CMD_CALC},
    {FLD_IDC, UNIT_A, CH4, 28, 2, 100},
    {FLD_PDC, UNIT_W, CH4, 32, 2, 10},
    {FLD_YD, UNIT_WH, CH4, 44, 2, 1},
    {FLD_YT, UNIT_KWH, CH4, 38, 4, 1000},
    {FLD_IRR, UNIT_PCT, CH4, CALC_IRR_CH, CH4, CMD_CALC},

    {FLD_UAC, UNIT_V, CH0, 46, 2, 10},
    {FLD_IAC, UNIT_A, CH0, 54, 2, 100},
    {FLD_PAC, UNIT_W, CH0, 50, 2, 10},
    {FLD_Q, UNIT_VAR, CH0, 52, 2, 10},
    {FLD_F, UNIT_HZ, CH0, 48, 2, 100},
    {FLD_PF, UNIT_NONE, CH0, 56, 2, 1000},
    {FLD_T, UNIT_C, CH0, 58, 2, 10},
    {FLD_EVT, UNIT_NONE, CH0, 60, 2, 1},
    {FLD_YD, UNIT_WH, CH0, CALC_YD_CH0, 0, CMD_CALC},
    {FLD_YT, UNIT_KWH, CH0, CALC_YT_CH0, 0, CMD_CALC},
    {FLD_PDC, UNIT_W, CH0, CALC_PDC_CH0, 0, CMD_CALC},
    {FLD_EFF, UNIT_PCT, CH0, CALC_EFF_CH0, 0, CMD_CALC}};
#define HM4CH_LIST_LEN (sizeof(hm4chAssignment) / sizeof(byteAssign_t))
#define HM4CH_PAYLOAD_LEN 62


/**
 * simple decoding of 1,2,4ch HM-inverter only
 */
static void decode_InfoREQResp(uint8_t _cmd, uint8_t *_user_payload, uint8_t _ulen, uint32_t _ts, char *_strout, uint8_t _strlen, uint16_t _invtype, uint32_t _plateID) {
    volatile int32_t _val = 0L;

    #if defined(ESP8266)
    // for esp8266 environment
    byteAssign_t _bp;                                                                  //volatile byteAssign_t not compiling ESP8266
    #else
    volatile byteAssign_t _bp;                                                         //must be volatile for Arduino-Nano, otherwise _bp.ch stays zero
    #endif

    volatile uint8_t _x;
    volatile uint8_t _end;
    volatile float _fval;
    volatile uint8_t _tmp8 = 0xff;

    volatile uint8_t LIST_LEN = 0;
    volatile uint8_t ixn = 0;

    //_str80[80] = '\0';
    snprintf_P(_strout, _strlen, PSTR("\ndata age: %d sec"), (millis() - _ts) / 1000);
    Serial.print(_strout);

    if (_cmd == 0x95) {

        //check given payload len
        if(_ulen == HM2CH_PAYLOAD_LEN) {
            ixn = 2;
            LIST_LEN = HM2CH_LIST_LEN;
        } else if (_ulen == HM4CH_PAYLOAD_LEN) {
            ixn = 4;
            LIST_LEN = HM4CH_LIST_LEN;
        } else if (_ulen == HM1CH_PAYLOAD_LEN) {
            ixn = 0;
            LIST_LEN = HM1CH_LIST_LEN;
        } else {
            Serial.print(F("ERR Wrong PL_LEN"));
            return;
        }

        for (_x = 0; _x < LIST_LEN; _x++) {
            // read values from given positions in payload
            _bp = hm2chAssignment[_x];
            _val = 0L;
            _end = _bp.start + _bp.num;
            if (_tmp8 != _bp.ch) {
                //snprintf_P(_strout, _strlen, PSTR("\nHM800/%04X%08lX/ch%02d "), _invtype, _plateID, _bp.ch);                            //must be a small "l" in %08lX for Arduino-Nano
                snprintf_P(_strout, _strlen, PSTR("\n%S/%04X%08lX/ch%02d "), PGM_invname[ixn],_invtype, _plateID, _bp.ch);                //must be a small "l" in %08lX for Arduino-Nano, https://forum.arduino.cc/t/f-macro-and-sprintf/894536/10
                Serial.print(_strout);
            }
            _tmp8 = _bp.ch;

            if (CMD_CALC != _bp.div && _bp.div != 0) {
                strncpy_P(_strout, &(PGM_fields[_bp.fieldId][0]), _strlen);
                Serial.print(_strout);
                Serial.print(F(": "));

                do {
                    _val <<= 8;
                    _val |= _user_payload[_bp.start];
                } while (++_bp.start != _end);
                if (_bp.num > 2) {
                    _fval = (int32_t)_val / (float)_bp.div;                                                                 //must be int32 to handle 4bit value of yield total
                } else {
                    _fval = (int16_t)_val / (float)_bp.div;                                                                 //for negative temparture value it must be int16
                }

                if (_bp.unitId == UNIT_NONE) {
                    Serial.print(_val);
                    Serial.print(F(" "));
                    continue;
                }
                Serial.print(_fval, 2);  // arduino nano does not sprintf(float values), but print() does
                // Serial.print(units[_bp.unitId]);
                strncpy_P(_strout, &PGM_units[_bp.unitId][0], _strlen);
                Serial.print(_strout);

            } else {
                // do calculations
                // Serial.print(F("not yet"));
            }
        }  // end for()
        Serial.println();

    } else {
        Serial.print(F("NO DECODER "));
        Serial.print(_cmd, HEX);
    }
}//end decodePayload()

#endif /*__HM_DEFINES_H__*/
