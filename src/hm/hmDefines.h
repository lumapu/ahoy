//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HM_DEFINES_H__
#define __HM_DEFINES_H__

#include "../utils/dbg.h"
#include <cstdint>

typedef enum {
    InverterDevInform_Simple = 0,  // 0x00
    InverterDevInform_All    = 1,  // 0x01
    GridOnProFilePara        = 2,  // 0x02
    HardWareConfig           = 3,  // 0x03
    SimpleCalibrationPara    = 4,  // 0x04
    SystemConfigPara         = 5,  // 0x05
    RealTimeRunData_Debug    = 11, // 0x0b
    RealTimeRunData_Reality  = 12, // 0x0c
    RealTimeRunData_A_Phase  = 13, // 0x0d
    RealTimeRunData_B_Phase  = 14, // 0x0e
    RealTimeRunData_C_Phase  = 15, // 0x0f
    AlarmData                = 17, // 0x11, Alarm data - all unsent alarms
    AlarmUpdate              = 18, // 0x12, Alarm data - all pending alarms
    RecordData               = 19, // 0x13
    InternalData             = 20, // 0x14
    GetLossRate              = 21, // 0x15
    GetSelfCheckState        = 30, // 0x1e
    InitDataState            = 0xff
} InfoCmdType;

typedef enum {
    TurnOn                  = 0,  // 0x00
    TurnOff                 = 1,  // 0x01
    Restart                 = 2,  // 0x02
    Lock                    = 3,  // 0x03
    Unlock                  = 4,  // 0x04
    ActivePowerContr        = 11, // 0x0b
    ReactivePowerContr      = 12, // 0x0c
    PFSet                   = 13, // 0x0d
    CleanState_LockAndAlarm = 20, // 0x14
    SelfInspection          = 40, // 0x28, self-inspection of grid-connected protection files
    Init                    = 0xff
} DevControlCmdType;

// inverter generations
enum {IV_MI = 0, IV_HM, IV_HMS, IV_HMT, IV_HERF, IV_UNKNOWN};
const char* const generationNames[] = {"MI", "HM", "HMS", "HMT", "HERF", "UNKNOWN"};

// units
enum {UNIT_V = 0, UNIT_A, UNIT_W,  UNIT_WH, UNIT_KWH, UNIT_HZ, UNIT_C, UNIT_PCT, UNIT_VAR, UNIT_NONE};
const char* const units[] = {"V", "A", "W", "Wh", "kWh", "Hz", "°C", "%", "var", ""};

// field types
enum {FLD_UDC = 0, FLD_IDC, FLD_PDC, FLD_YD, FLD_YW, FLD_YT,
        FLD_UAC, FLD_UAC_1N, FLD_UAC_2N, FLD_UAC_3N, FLD_UAC_12, FLD_UAC_23, FLD_UAC_31, FLD_IAC,
        FLD_IAC_1, FLD_IAC_2, FLD_IAC_3, FLD_PAC, FLD_F, FLD_T, FLD_PF, FLD_EFF,
        FLD_IRR, FLD_Q, FLD_EVT, FLD_FW_VERSION, FLD_FW_BUILD_YEAR,
        FLD_FW_BUILD_MONTH_DAY, FLD_FW_BUILD_HOUR_MINUTE, FLD_BOOTLOADER_VER,
        FLD_ACT_ACTIVE_PWR_LIMIT, FLD_PART_NUM, FLD_HW_VERSION, FLD_GRID_PROFILE_CODE,
        FLD_GRID_PROFILE_VERSION,  /*FLD_ACT_REACTIVE_PWR_LIMIT, FLD_ACT_PF,*/ FLD_LAST_ALARM_CODE, FLD_MP, FLD_MT};

const char* const fields[] = {"U_DC", "I_DC", "P_DC", "YieldDay", "YieldWeek", "YieldTotal",
        "U_AC", "U_AC_1N", "U_AC_2N", "U_AC_3N", "U_AC_12", "U_AC_23", "U_AC_31", "I_AC",
        "I_AC_1", "I_AC_2", "I_AC_3", "P_AC", "F_AC", "Temp", "PF_AC", "Efficiency", "Irradiation","Q_AC",
        "ALARM_MES_ID","FWVersion","FWBuildYear","FWBuildMonthDay","FWBuildHourMinute","BootloaderVersion",
        "active_PowerLimit", "HWPartNumber", "HWVersion", "GridProfileCode",
        "GridProfileVersion", /*"reactivePowerLimit","Powerfactor",*/ "LastAlarmCode", "MaxPower", "MaxTemp"};
const char* const notAvail = "n/a";

const uint8_t fieldUnits[] = {UNIT_V, UNIT_A, UNIT_W, UNIT_WH, UNIT_KWH, UNIT_KWH,
        UNIT_V, UNIT_V, UNIT_V, UNIT_V, UNIT_V, UNIT_V, UNIT_V, UNIT_A, UNIT_A, UNIT_A, UNIT_A,
        UNIT_W, UNIT_HZ, UNIT_C, UNIT_NONE, UNIT_PCT, UNIT_PCT, UNIT_VAR,
        UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_PCT, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_W, UNIT_C};

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
    {FLD_F,   DEVICE_CLS_FREQ,    STATE_CLS_MEASUREMENT},
    {FLD_T,   DEVICE_CLS_TEMP,    STATE_CLS_MEASUREMENT},
    {FLD_PF,  DEVICE_CLS_NONE,    STATE_CLS_NONE},
    {FLD_EFF, DEVICE_CLS_NONE,    STATE_CLS_NONE},
    {FLD_IRR, DEVICE_CLS_NONE,    STATE_CLS_NONE}
};
#define DEVICE_CLS_ASSIGN_LIST_LEN     (sizeof(deviceFieldAssignment) / sizeof(byteAssign_fieldDeviceClass))

// indices to calculation functions, defined in hmInverter.h
enum {CALC_YT_CH0 = 0, CALC_YD_CH0, CALC_UDC_CH, CALC_PDC_CH0, CALC_EFF_CH0, CALC_IRR_CH, CALC_MPAC_CH0, CALC_MPDC_CH, CALC_MT_CH0};
enum {CMD_CALC = 0xffff};


// CH0 is default channel (freq, ac, temp)
enum {CH0 = 0, CH1, CH2, CH3, CH4, CH5, CH6};

enum {INV_TYPE_1CH = 0, INV_TYPE_2CH, INV_TYPE_4CH, INV_TYPE_6CH};
enum {INV_RADIO_TYPE_UNKNOWN = 0, INV_RADIO_TYPE_NRF, INV_RADIO_TYPE_CMT};


#define DURATION_ONEFRAME       50 // timeout parameter for each expected frame (ms)
//#define DURATION_RESERVE  {90,120} // timeout parameter to still wait after last expected frame (ms)
#define DURATION_TXFRAME        85 // timeout parameter for first transmission and first expected frame (time to first channel switch from tx start!) (ms)
#define DURATION_LISTEN_MIN      5 // time to stay at least on a listening channel (ms)
#define DURATION_PAUSE_LASTFR   45 // how long to pause after last frame (ms)
const uint8_t duration_reserve[2] = {65, 115};

#define LIMIT_FAST_IV           85 // time limit to qualify an inverter as very fast answering inverter
#define LIMIT_VERYFAST_IV       70 // time limit to qualify an inverter as very fast answering inverter
#define LIMIT_FAST_IV_MI        35 // time limit to qualify a MI type inverter as fast answering inverter
#define LIMIT_VERYFAST_IV_MI    25 // time limit to qualify a MI type inverter as very fast answering inverter
#define RETRIES_FAST_IV         12 // how often shall a message be automatically retransmitted by the nRF (fast answering inverter)
#define RETRIES_VERYFAST_IV      9 // how often shall a message be automatically retransmitted by the nRF (very fast answering inverter)


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
// HM-Series
//-------------------------------------
const byteAssign_t InfoAssignment[] = {
    { FLD_FW_VERSION,           UNIT_NONE,   CH0,  0, 2, 1 },
    { FLD_FW_BUILD_YEAR,        UNIT_NONE,   CH0,  2, 2, 1 },
    { FLD_FW_BUILD_MONTH_DAY,   UNIT_NONE,   CH0,  4, 2, 1 },
    { FLD_FW_BUILD_HOUR_MINUTE, UNIT_NONE,   CH0,  6, 2, 1 },
    { FLD_BOOTLOADER_VER,       UNIT_NONE,   CH0,  8, 2, 1 }
};
#define HMINFO_LIST_LEN     (sizeof(InfoAssignment) / sizeof(byteAssign_t))
#define HMINFO_PAYLOAD_LEN  14

const byteAssign_t SimpleInfoAssignment[] = {
    { FLD_PART_NUM,             UNIT_NONE,   CH0,  2, 4, 1 },
    { FLD_HW_VERSION,           UNIT_NONE,   CH0,  6, 2, 1 },
    { FLD_GRID_PROFILE_CODE,    UNIT_NONE,   CH0,  8, 2, 1 },
    { FLD_GRID_PROFILE_VERSION, UNIT_NONE,   CH0, 10, 2, 1 }
};
#define HMSIMPLE_INFO_LIST_LEN     (sizeof(SimpleInfoAssignment) / sizeof(byteAssign_t))
#define HMSIMPLE_INFO_PAYLOAD_LEN  14

const byteAssign_t SystemConfigParaAssignment[] = {
    { FLD_ACT_ACTIVE_PWR_LIMIT,    UNIT_PCT,   CH0,  2, 2, 10   }/*,
    { FLD_ACT_REACTIVE_PWR_LIMIT,  UNIT_PCT,   CH0,  4, 2, 10   },
    { FLD_ACT_PF,                  UNIT_NONE,  CH0,  6, 2, 1000 }*/
};
#define HMSYSTEM_LIST_LEN     (sizeof(SystemConfigParaAssignment) / sizeof(byteAssign_t))
#define HMSYSTEM_PAYLOAD_LEN  14

const byteAssign_t AlarmDataAssignment[] = {
    { FLD_LAST_ALARM_CODE,           UNIT_NONE,   CH0,  0, 2, 1 }
};
#define HMALARMDATA_LIST_LEN     (sizeof(AlarmDataAssignment) / sizeof(byteAssign_t))
#define HMALARMDATA_PAYLOAD_LEN  0 // 0: means check is off
#define ALARM_LOG_ENTRY_SIZE    12

#define HMGETLOSSRATE_PAYLOAD_LEN 4
#define AHOY_GET_LOSS_INTERVAL 10

//-------------------------------------
// HM300, HM350, HM400
//-------------------------------------
const byteAssign_t hm1chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,    CH1,  4, 2, 100  },
    { FLD_PDC, UNIT_W,    CH1,  6, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH1, 12, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH1,  8, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 14, 2, 10   },
    { FLD_IAC, UNIT_A,    CH0, 22, 2, 100  },
    { FLD_PAC, UNIT_W,    CH0, 18, 2, 10   },
    { FLD_Q,   UNIT_VAR,  CH0, 20, 2, 10   },
    { FLD_F,   UNIT_HZ,   CH0, 16, 2, 100  },
    { FLD_PF,  UNIT_NONE, CH0, 24, 2, 1000 },
    { FLD_T,   UNIT_C,    CH0, 26, 2, 10   },
    { FLD_EVT, UNIT_NONE, CH0, 28, 2, 1    },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC },
    { FLD_MT,  UNIT_C,    CH0, CALC_MT_CH0,   0, CMD_CALC }
};
#define HM1CH_LIST_LEN      (sizeof(hm1chAssignment) / sizeof(byteAssign_t))
#define HM1CH_PAYLOAD_LEN   30


//-------------------------------------
// HM600, HM700, HM800
//-------------------------------------
const byteAssign_t hm2chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,    CH1,  4, 2, 100  },
    { FLD_PDC, UNIT_W,    CH1,  6, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH1, 22, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH1, 14, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH2,  8, 2, 10   },
    { FLD_IDC, UNIT_A,    CH2, 10, 2, 100  },
    { FLD_PDC, UNIT_W,    CH2, 12, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH2, 24, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH2, 18, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH2, CALC_IRR_CH, CH2, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH2, CALC_MPDC_CH, CH2, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 26, 2, 10   },
    { FLD_IAC, UNIT_A,    CH0, 34, 2, 100  },
    { FLD_PAC, UNIT_W,    CH0, 30, 2, 10   },
    { FLD_Q,   UNIT_VAR,  CH0, 32, 2, 10   },
    { FLD_F,   UNIT_HZ,   CH0, 28, 2, 100  },
    { FLD_PF,  UNIT_NONE, CH0, 36, 2, 1000 },
    { FLD_T,   UNIT_C,    CH0, 38, 2, 10   },
    { FLD_EVT, UNIT_NONE, CH0, 40, 2, 1    },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC },
    { FLD_MT,  UNIT_C,    CH0, CALC_MT_CH0,   0, CMD_CALC }

};
#define HM2CH_LIST_LEN      (sizeof(hm2chAssignment) / sizeof(byteAssign_t))
#define HM2CH_PAYLOAD_LEN   42


//-------------------------------------
// HM1200, HM1500
//-------------------------------------
const byteAssign_t hm4chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,    CH1,  4, 2, 100  },
    { FLD_PDC, UNIT_W,    CH1,  8, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH1, 20, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH1, 12, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH2, CALC_UDC_CH, CH1, CMD_CALC },
    { FLD_IDC, UNIT_A,    CH2,  6, 2, 100  },
    { FLD_PDC, UNIT_W,    CH2, 10, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH2, 22, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH2, 16, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH2, CALC_IRR_CH, CH2, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH2, CALC_MPDC_CH, CH2, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH3, 24, 2, 10   },
    { FLD_IDC, UNIT_A,    CH3, 26, 2, 100  },
    { FLD_PDC, UNIT_W,    CH3, 30, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH3, 42, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH3, 34, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH3, CALC_IRR_CH, CH3, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH3, CALC_MPDC_CH, CH3, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH4, CALC_UDC_CH, CH3, CMD_CALC },
    { FLD_IDC, UNIT_A,    CH4, 28, 2, 100  },
    { FLD_PDC, UNIT_W,    CH4, 32, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH4, 44, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH4, 38, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH4, CALC_IRR_CH, CH4, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH4, CALC_MPDC_CH, CH4, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 46, 2, 10   },
    { FLD_IAC, UNIT_A,    CH0, 54, 2, 100  },
    { FLD_PAC, UNIT_W,    CH0, 50, 2, 10   },
    { FLD_Q,   UNIT_VAR,  CH0, 52, 2, 10   },
    { FLD_F,   UNIT_HZ,   CH0, 48, 2, 100  },
    { FLD_PF,  UNIT_NONE, CH0, 56, 2, 1000 },
    { FLD_T,   UNIT_C,    CH0, 58, 2, 10   },
    { FLD_EVT, UNIT_NONE, CH0, 60, 2, 1    },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC },
    { FLD_MT,  UNIT_C,    CH0, CALC_MT_CH0,   0, CMD_CALC }
};
#define HM4CH_LIST_LEN      (sizeof(hm4chAssignment) / sizeof(byteAssign_t))
#define HM4CH_PAYLOAD_LEN   62

//-------------------------------------
// HERF300, HERF400, HERF500
//-------------------------------------
const byteAssign_t herf1chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,    CH1,  6, 2, 100  },
    { FLD_PDC, UNIT_W,    CH1, 10, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH1, 22, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH1, 14, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 26, 2, 10   },
    { FLD_IAC, UNIT_A,    CH0, 34, 2, 100  },
    { FLD_PAC, UNIT_W,    CH0, 30, 2, 10   },
    { FLD_Q,   UNIT_VAR,  CH0, 40, 2, 10   },
    { FLD_F,   UNIT_HZ,   CH0, 28, 2, 100  },
    { FLD_PF,  UNIT_NONE, CH0, 36, 2, 1000 },
    { FLD_T,   UNIT_C,    CH0, 38, 2, 10   },
    { FLD_EVT, UNIT_NONE, CH0, 40, 2, 1    },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC },
    { FLD_MT,  UNIT_C,    CH0, CALC_MT_CH0,   0, CMD_CALC }
};
#define HERF1CH_LIST_LEN      (sizeof(herf1chAssignment) / sizeof(byteAssign_t))
#define HERF1CH_PAYLOAD_LEN   30

//-------------------------------------
// HERF600, HERF800
//-------------------------------------
const byteAssign_t herf2chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2, 10   },
    { FLD_IDC, UNIT_A,    CH1,  6, 2, 100  },
    { FLD_PDC, UNIT_W,    CH1, 10, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH1, 22, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH1, 14, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH2,  4, 2, 10   },
    { FLD_IDC, UNIT_A,    CH2,  8, 2, 100  },
    { FLD_PDC, UNIT_W,    CH2, 12, 2, 10   },
    { FLD_YD,  UNIT_WH,   CH2, 24, 2, 1    },
    { FLD_YT,  UNIT_KWH,  CH2, 18, 4, 1000 },
    { FLD_IRR, UNIT_PCT,  CH2, CALC_IRR_CH, CH2, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH2, CALC_MPDC_CH, CH2, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 26, 2, 10   },
    { FLD_IAC, UNIT_A,    CH0, 34, 2, 100  },
    { FLD_PAC, UNIT_W,    CH0, 30, 2, 10   },
    { FLD_Q,   UNIT_VAR,  CH0, 32, 2, 10   },
    { FLD_F,   UNIT_HZ,   CH0, 28, 2, 100  },
    { FLD_PF,  UNIT_NONE, CH0, 36, 2, 1000 },
    { FLD_T,   UNIT_C,    CH0, 38, 2, 10   },
    { FLD_EVT, UNIT_NONE, CH0, 40, 2, 1    },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC },
    { FLD_MT,  UNIT_C,    CH0, CALC_MT_CH0,   0, CMD_CALC }

};
#define HERF2CH_LIST_LEN      (sizeof(herf2chAssignment) / sizeof(byteAssign_t))
#define HERF2CH_PAYLOAD_LEN   42

typedef struct {
    uint32_t hwPart;
    uint16_t maxPower;
} devInfo_t;

// https://github.com/lumapu/ahoy/issues/1111
// Hardware number:
// 0xAABBCCDD
//   ^^ ------- always 10 (for MI, HM, HMS, HMT)
//     ^ ------ 0 = MI
//              1 = HM
//              1, 2 = HMS (version)
//              3 = HMT
//      ^ ----- 0 = 1 Input
//              1 = 2 Inputs
//              2 = 4 Inputs
//              3 = 6 Inputs
//       ^ ---- 0 = smallest with x inputs
//              7 = biggest with x inputs

const devInfo_t devInfo[] = {
    // MI 2nd gen; only 0x001311 is tested,
    //                  others (starting with MI-250) according to https://github.com/lumapu/ahoy/issues/1111#issuecomment-1698100571
    { 0x000111,  250 },
    { 0x000311,  300 },
    { 0x000411,  350 },
    { 0x001111,  500 },
    { 0x001311,  600 },
    { 0x001321,  600 },
    { 0x001421,  700 },
    { 0x001411,  700 },
    { 0x002111, 1000 },
    { 0x002311, 1200 },
    { 0x002511, 1500 },
    { 0x002411, 1500 },

    // MI 3rd gen
    { 0x100000,  250 },
    { 0x100010,  300 },
    { 0x100020,  350 },
    { 0x100030,  400 },
    { 0x100100,  500 },
    { 0x100110,  600 },
    { 0x100120,  700 },
    { 0x100130,  800 },
    { 0x100200, 1000 },
    { 0x100210, 1200 },
    { 0x100230, 1500 },

    // HM
    { 0x101010,  300 },
    { 0x101020,  350 },
    { 0x101030,  400 },
    { 0x101040,  400 },
    { 0x101110,  600 }, // [TSOL800(DE) ..20, HWv=2.66], [HM-600 ..20, HWv=2.66]
    { 0x101120,  700 },
    { 0x101130,  800 },
    { 0x101140,  800 },
    { 0x101200, 1000 },
    { 0x101210, 1200 },
    { 0x101230, 1500 },

    // HMS
    { 0x102021,  350 },
    { 0x102041,  400 },
    { 0x101051,  450 },
    { 0x101071,  500 },
    { 0x102111,  600 },
    { 0x101120,  700 },
    { 0x102141,  800 },
    { 0x101151,  900 },
    { 0x102171, 1000 },
    { 0x102241, 1600 },
    { 0x101251, 1800 },
    { 0x102251, 1800 },
    { 0x101271, 2000 }, // v1 grey backplane, 14A
    { 0x102271, 2000 }, // v2 black backplane, 16A

    // HMT
    { 0x103241, 1600 }, // -4T
    { 0x103251, 1800 }, // -4T
    { 0x103271, 2000 }, // -4T
    { 0x103311, 1800 }, // -6T
    { 0x103331, 2250 }  // -6T
};

#define MI_REQ_CH1 0x09
#define MI_REQ_CH2 0x11
#define MI_REQ_4CH 0x36

#endif /*__HM_DEFINES_H__*/
