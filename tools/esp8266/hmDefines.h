//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HM_DEFINES_H__
#define __HM_DEFINES_H__

#include "dbg.h"
#include <cstdint>


union serial_u {
    uint64_t u64;
    uint8_t  b[8];
};


// units
enum {UNIT_V = 0, UNIT_A, UNIT_W,  UNIT_WH, UNIT_KWH, UNIT_HZ, UNIT_C, UNIT_PCT};
const char* const units[] = {"V", "A", "W", "Wh", "kWh", "Hz", "°C", "%"};


// field types
enum {FLD_UDC = 0, FLD_IDC, FLD_PDC, FLD_YD, FLD_YW, FLD_YT,
        FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_T, FLD_PCT, FLD_EFF, FLD_IRR};
const char* const fields[] = {"U_DC", "I_DC", "P_DC", "YieldDay", "YieldWeek", "YieldTotal",
        "U_AC", "I_AC", "P_AC", "Freq", "Temp", "Pct", "Efficiency", "Irradiation"};

// mqtt discovery device classes
enum {DEVICE_CLS_NONE = 0, DEVICE_CLS_CURRENT, DEVICE_CLS_ENERGY, DEVICE_CLS_PWR, DEVICE_CLS_VOLTAGE, DEVICE_CLS_FREQ, DEVICE_CLS_TEMP};
const char* const deviceClasses[] = {0, "current", "energy", "power", "voltage", "frequency", "temperature"};
enum {STATE_CLS_NONE = 0, STATE_CLS_MEASUREMENT, STATE_CLS_TOTAL_INCREASING};
const char* const stateClasses[] = {0, "measurement", "total_increasing"};
typedef struct {
    uint8_t    fieldId;      // field id
    uint8_t    deviceClsId;  // device class
    uint8_t    stateClsId;   // state class
} byteAssign_fieldDeviceClass;
const byteAssign_fieldDeviceClass deviceFieldAssignment[] = {
    {FLD_UDC, DEVICE_CLS_VOLTAGE, STATE_CLS_MEASUREMENT},
    {FLD_IDC, DEVICE_CLS_CURRENT, STATE_CLS_MEASUREMENT},
    {FLD_PDC, DEVICE_CLS_PWR,     STATE_CLS_MEASUREMENT},
    {FLD_YD,  DEVICE_CLS_ENERGY,  STATE_CLS_TOTAL_INCREASING},
    {FLD_YW,  DEVICE_CLS_ENERGY,  STATE_CLS_TOTAL_INCREASING},
    {FLD_YT,  DEVICE_CLS_ENERGY,  STATE_CLS_TOTAL_INCREASING},
    {FLD_UAC, DEVICE_CLS_VOLTAGE, STATE_CLS_MEASUREMENT},
    {FLD_IAC, DEVICE_CLS_CURRENT, STATE_CLS_MEASUREMENT},
    {FLD_PAC, DEVICE_CLS_PWR,     STATE_CLS_MEASUREMENT},
    {FLD_F,   DEVICE_CLS_FREQ,    STATE_CLS_NONE},
    {FLD_T,   DEVICE_CLS_TEMP,    STATE_CLS_MEASUREMENT},
    {FLD_PCT, DEVICE_CLS_NONE,    STATE_CLS_NONE},
    {FLD_EFF, DEVICE_CLS_NONE,    STATE_CLS_NONE},
    {FLD_IRR, DEVICE_CLS_NONE,    STATE_CLS_NONE}
};
#define DEVICE_CLS_ASSIGN_LIST_LEN     (sizeof(deviceFieldAssignment) / sizeof(byteAssign_fieldDeviceClass))

// indices to calculation functions, defined in hmInverter.h
enum {CALC_YT_CH0 = 0, CALC_YD_CH0, CALC_UDC_CH, CALC_PDC_CH0, CALC_EFF_CH0, CALC_IRR_CH};
enum {CMD_CALC = 0xffff};


// CH0 is default channel (freq, ac, temp)
enum {CH0 = 0, CH1, CH2, CH3, CH4};

enum {INV_TYPE_1CH = 0, INV_TYPE_2CH, INV_TYPE_4CH};


typedef struct {
    uint8_t    fieldId; // field id
    uint8_t    unitId;  // uint id
    uint8_t    ch;      // channel 0 - 4
    uint8_t    start;   // pos of first byte in buffer
    uint8_t    num;     // number of bytes in buffer
    uint16_t   div;     // divisor / calc command
} byteAssign_t;


/**
 *  indices are built for the buffer starting with cmd-id in first byte
 *  (complete payload in buffer)
 * */

//-------------------------------------
// HM300, HM350, HM400
//-------------------------------------
const byteAssign_t hm1chAssignment[] = {
    { FLD_UDC, UNIT_V,   CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,   CH1,  4, 2, 100  },
    { FLD_PDC, UNIT_W,   CH1,  6, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH1, 12, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH1,  8, 4, 1000 },
    { FLD_IRR, UNIT_PCT, CH1, CALC_IRR_CH, CH1, CMD_CALC },

    { FLD_UAC, UNIT_V,   CH0, 14, 2, 10   },
    { FLD_IAC, UNIT_A,   CH0, 22, 2, 100  },
    { FLD_PAC, UNIT_W,   CH0, 18, 2, 10   },
    { FLD_F,   UNIT_HZ,  CH0, 16, 2, 100  },
    { FLD_T,   UNIT_C,   CH0, 26, 2, 10   },
    { FLD_PDC, UNIT_W,   CH0, CALC_PDC_CH0, 0, CMD_CALC },
    { FLD_EFF, UNIT_PCT, CH0, CALC_EFF_CH0, 0, CMD_CALC }
};
#define HM1CH_LIST_LEN     (sizeof(hm1chAssignment) / sizeof(byteAssign_t))


//-------------------------------------
// HM600, HM700, HM800
//-------------------------------------
const byteAssign_t hm2chAssignment[] = {
    { FLD_UDC, UNIT_V,   CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,   CH1,  4, 2, 100  },
    { FLD_PDC, UNIT_W,   CH1,  6, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH1, 22, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH1, 14, 4, 1000 },
    { FLD_IRR, UNIT_PCT, CH1, CALC_IRR_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH2,  8, 2, 10   },
    { FLD_IDC, UNIT_A,   CH2, 10, 2, 100  },
    { FLD_PDC, UNIT_W,   CH2, 12, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH2, 24, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH2, 18, 4, 1000 },
    { FLD_IRR, UNIT_PCT, CH2, CALC_IRR_CH, CH2, CMD_CALC },

    { FLD_UAC, UNIT_V,   CH0, 26, 2, 10   },
    { FLD_IAC, UNIT_A,   CH0, 34, 2, 100  },
    { FLD_PAC, UNIT_W,   CH0, 30, 2, 10   },
    { FLD_F,   UNIT_HZ,  CH0, 28, 2, 100  },
    { FLD_T,   UNIT_C,   CH0, 38, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH0, CALC_YD_CH0,  0, CMD_CALC },
    { FLD_YT,  UNIT_KWH, CH0, CALC_YT_CH0,  0, CMD_CALC },
    { FLD_PDC, UNIT_W,   CH0, CALC_PDC_CH0, 0, CMD_CALC },
    { FLD_EFF, UNIT_PCT, CH0, CALC_EFF_CH0, 0, CMD_CALC }

};
#define HM2CH_LIST_LEN     (sizeof(hm2chAssignment) / sizeof(byteAssign_t))


//-------------------------------------
// HM1200, HM1500
//-------------------------------------
const byteAssign_t hm4chAssignment[] = {
    { FLD_UDC, UNIT_V,   CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,   CH1,  4, 2, 100  },
    { FLD_PDC, UNIT_W,   CH1,  8, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH1, 20, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH1, 12, 4, 1000 },
    { FLD_IRR, UNIT_PCT, CH1, CALC_IRR_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH2, CALC_UDC_CH, CH1, CMD_CALC },
    { FLD_IDC, UNIT_A,   CH2,  6, 2, 100  },
    { FLD_PDC, UNIT_W,   CH2, 10, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH2, 22, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH2, 16, 4, 1000 },
    { FLD_IRR, UNIT_PCT, CH2, CALC_IRR_CH, CH2, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH3, 24, 2, 10   },
    { FLD_IDC, UNIT_A,   CH3, 26, 2, 100  },
    { FLD_PDC, UNIT_W,   CH3, 30, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH3, 42, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH3, 34, 4, 1000 },
    { FLD_IRR, UNIT_PCT, CH3, CALC_IRR_CH, CH3, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH4, CALC_UDC_CH, CH3, CMD_CALC },
    { FLD_IDC, UNIT_A,   CH4, 28, 2, 100  },
    { FLD_PDC, UNIT_W,   CH4, 32, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH4, 44, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH4, 38, 4, 1000 },
    { FLD_IRR, UNIT_PCT, CH4, CALC_IRR_CH, CH4, CMD_CALC },

    { FLD_UAC, UNIT_V,   CH0, 46, 2, 10   },
    { FLD_IAC, UNIT_A,   CH0, 54, 2, 100  },
    { FLD_PAC, UNIT_W,   CH0, 50, 2, 10   },
    { FLD_F,   UNIT_HZ,  CH0, 48, 2, 100  },
    { FLD_PCT, UNIT_PCT, CH0, 56, 2, 10   },
    { FLD_T,   UNIT_C,   CH0, 58, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH0, CALC_YD_CH0,  0, CMD_CALC },
    { FLD_YT,  UNIT_KWH, CH0, CALC_YT_CH0,  0, CMD_CALC },
    { FLD_PDC, UNIT_W,   CH0, CALC_PDC_CH0, 0, CMD_CALC },
    { FLD_EFF, UNIT_PCT, CH0, CALC_EFF_CH0, 0, CMD_CALC }
};
#define HM4CH_LIST_LEN     (sizeof(hm4chAssignment) / sizeof(byteAssign_t))


#endif /*__HM_DEFINES_H__*/
