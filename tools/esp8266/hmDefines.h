#ifndef __HM_DEFINES_H__
#define __HM_DEFINES_H__

#include "debug.h"
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
        FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_T, FLD_PCT};
const char* const fields[] = {"U_DC", "I_DC", "P_DC", "YieldDay", "YieldWeek", "YieldTotal",
        "U_AC", "I_AC", "P_AC", "Freq", "Temp", "Pct"};


// indices to calculation functions, defined in hmInverter.h
enum {CALC_YT_CH0 = 0, CALC_YD_CH0, CALC_UDC_CH};


// CH0 is default channel (freq, ac, temp)
enum {CH0 = 0, CH1, CH2, CH3, CH4};
// received command ids, special command CMDFF for calculations
enum {CMD01 = 0x01, CMD02, CMD03, CMD82 = 0x82, CMD83, CMD84, CMDFF=0xff};

enum {INV_TYPE_HM600 = 0, INV_TYPE_HM1200, INV_TYPE_HM400, INV_TYPE_HM800};
const char* const invTypes[] = {"HM600", "HM1200 / HM1500", "HM400", "HM800"};
#define NUM_INVERTER_TYPES   4


typedef struct {
    uint8_t    fieldId; // field id
    uint8_t    unitId;  // uint id
    uint8_t    ch;      // channel 0 - 3
    uint8_t    cmdId;   // received command id
    uint8_t    start;   // pos of first byte in buffer
    uint8_t    num;     // number of bytes in buffer
    uint16_t   div;     // divisor
} byteAssign_t;


/**
 *  indices are built for the buffer starting with cmd-id in first byte
 *  (complete payload in buffer)
 * */

//-------------------------------------
// HM400 HM350?, HM300?
//-------------------------------------
const byteAssign_t hm400assignment[] = {
    { FLD_UDC, UNIT_V,   CH1, CMD01,  3, 2, 10   },
    { FLD_IDC, UNIT_A,   CH1, CMD01,  5, 2, 100  },
    { FLD_PDC, UNIT_W,   CH1, CMD01,  7, 2, 10   },
    { FLD_YT,  UNIT_KWH, CH1, CMD01,  9, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH1, CMD01, 13, 2, 1    },
    { FLD_UAC, UNIT_V,   CH0, CMD01, 15, 2, 10   },
    { FLD_F,   UNIT_HZ,  CH0, CMD82,  1, 2, 100  },
    { FLD_PAC, UNIT_W,   CH0, CMD82,  3, 2, 10   },
    { FLD_IAC, UNIT_A,   CH0, CMD82,  7, 2, 100  },
    { FLD_T,   UNIT_C,   CH0, CMD82, 11, 2, 10   }
};
#define HM400_LIST_LEN     (sizeof(hm400assignment) / sizeof(byteAssign_t))


//-------------------------------------
// HM600, HM700
//-------------------------------------
const byteAssign_t hm600assignment[] = {
    { FLD_UDC, UNIT_V,   CH1, CMD01,  3, 2, 10   },
    { FLD_IDC, UNIT_A,   CH1, CMD01,  5, 2, 100  },
    { FLD_PDC, UNIT_W,   CH1, CMD01,  7, 2, 10   },
    { FLD_UDC, UNIT_V,   CH2, CMD01,  9, 2, 10   },
    { FLD_IDC, UNIT_A,   CH2, CMD01, 11, 2, 100  },
    { FLD_PDC, UNIT_W,   CH2, CMD01, 13, 2, 10   },
    { FLD_YW,  UNIT_WH,  CH0, CMD02,  1, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH0, CMD02,  3, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH1, CMD02,  7, 2, 1    },
    { FLD_YD,  UNIT_WH,  CH2, CMD02,  9, 2, 1    },
    { FLD_UAC, UNIT_V,   CH0, CMD02, 11, 2, 10   },
    { FLD_F,   UNIT_HZ,  CH0, CMD02, 13, 2, 100  },
    { FLD_PAC, UNIT_W,   CH0, CMD02, 15, 2, 10   },
    { FLD_IAC, UNIT_A,   CH0, CMD83,  3, 2, 100  },
    { FLD_T,   UNIT_C,   CH0, CMD83,  7, 2, 10   }
};
#define HM600_LIST_LEN     (sizeof(hm600assignment) / sizeof(byteAssign_t))


//-------------------------------------
// HM800
//-------------------------------------
const byteAssign_t hm800assignment[] = {

    { FLD_UDC, UNIT_V,   CH1, CMD01,  3, 2, 10   },
    { FLD_IDC, UNIT_A,   CH1, CMD01,  5, 2, 100  },
    { FLD_PDC, UNIT_W,   CH1, CMD01,  7, 2, 10   },
    { FLD_UDC, UNIT_V,   CH2, CMD01,  9, 2, 10   },
    { FLD_IDC, UNIT_A,   CH2, CMD01, 11, 2, 100  },
    { FLD_PDC, UNIT_W,   CH2, CMD01, 13, 2, 10   },
    { FLD_YW,  UNIT_WH,  CH0, CMD02,  1, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH0, CMD02,  3, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH1, CMD02,  7, 2, 1    },
    { FLD_YD,  UNIT_WH,  CH2, CMD02,  9, 2, 1    },
    { FLD_UAC, UNIT_V,   CH0, CMD02, 11, 2, 10   },
    { FLD_F,   UNIT_HZ,  CH0, CMD02, 13, 2, 100  },
    { FLD_PAC, UNIT_W,   CH0, CMD02, 15, 2, 10   },
    { FLD_IAC, UNIT_A,   CH0, CMD83,  3, 2, 100  },
    { FLD_T,   UNIT_C,   CH0, CMD83,  7, 2, 10   }
};
#define HM800_LIST_LEN     (sizeof(hm800assignment) / sizeof(byteAssign_t))


//-------------------------------------
// HM1200, HM1500
//-------------------------------------
const byteAssign_t hm1200assignment[] = {
    { FLD_UDC, UNIT_V,   CH1, CMD01,  3, 2, 10   },
    { FLD_IDC, UNIT_A,   CH1, CMD01,  5, 2, 100  },
    { FLD_PDC, UNIT_W,   CH1, CMD01,  9, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH1, CMD02,  5, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH1, CMD01, 13, 4, 1000 },
    { FLD_UDC, UNIT_V,   CH3, CMD02,  9, 2, 10   },
    { FLD_IDC, UNIT_A,   CH2, CMD01,  7, 2, 100  },
    { FLD_PDC, UNIT_W,   CH2, CMD01, 11, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH2, CMD02,  7, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH2, CMD02,  1, 4, 1000 },
    { FLD_IDC, UNIT_A,   CH3, CMD02, 11, 2, 100  },
    { FLD_PDC, UNIT_W,   CH3, CMD02, 15, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH3, CMD03, 11, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH3, CMD03,  3, 4, 1000 },
    { FLD_IDC, UNIT_A,   CH4, CMD02, 13, 2, 100  },
    { FLD_PDC, UNIT_W,   CH4, CMD03,  1, 2, 10   },
    { FLD_YD,  UNIT_WH,  CH4, CMD03, 13, 2, 1    },
    { FLD_YT,  UNIT_KWH, CH4, CMD03,  7, 4, 1000 },
    { FLD_UAC, UNIT_V,   CH0, CMD03, 15, 2, 10   },
    { FLD_IAC, UNIT_A,   CH0, CMD84,  7, 2, 100  },
    { FLD_PAC, UNIT_W,   CH0, CMD84,  3, 2, 10   },
    { FLD_F,   UNIT_HZ,  CH0, CMD84,  1, 2, 100  },
    { FLD_PCT, UNIT_PCT, CH0, CMD84,  9, 2, 10   },
    { FLD_T,   UNIT_C,   CH0, CMD84, 11, 2, 10   },
    { FLD_YT,  UNIT_KWH, CH0, CMDFF, CALC_YT_CH0,   0, 0 },
    { FLD_YD,  UNIT_KWH, CH0, CMDFF, CALC_YD_CH0,   0, 0 },
    { FLD_UDC, UNIT_V,   CH2, CMDFF, CALC_UDC_CH, CH1, 0 },
    { FLD_UDC, UNIT_V,   CH4, CMDFF, CALC_UDC_CH, CH3, 0 }
};
#define HM1200_LIST_LEN     (sizeof(hm1200assignment) / sizeof(byteAssign_t))




#endif /*__HM_DEFINES_H__*/
