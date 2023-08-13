//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HMS_DEFINES_H__
#define __HMS_DEFINES_H__

#include "../hm/hmDefines.h"

//-------------------------------------
// HMS-350, HMS-500
//-------------------------------------
const byteAssign_t hms1chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2,   10 },
    { FLD_IDC, UNIT_A,    CH1,  4, 2,  100 },
    { FLD_PDC, UNIT_W,    CH1,  6, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH1,  8, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH1, 12, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 14, 2,   10 },
    { FLD_F,   UNIT_HZ,   CH0, 16, 2,  100 },
    { FLD_PAC, UNIT_W,    CH0, 18, 2,   10 },
    { FLD_Q,   UNIT_VAR,  CH0, 20, 2,   10 }, // signed!
    { FLD_IAC, UNIT_A,    CH0, 22, 2,  100 },
    { FLD_PF,  UNIT_NONE, CH0, 24, 2, 1000 }, // signed!
    { FLD_T,   UNIT_C,    CH0, 26, 2,   10 }, // signed!
    { FLD_EVT, UNIT_NONE, CH0, 28, 2,    1 },

    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC }
};
#define HMS1CH_LIST_LEN      (sizeof(hms1chAssignment) / sizeof(byteAssign_t))
#define HMS1CH_PAYLOAD_LEN   30

//-------------------------------------
// HMS-800, HMS-1000
//-------------------------------------
const byteAssign_t hms2chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2,   10 },
    { FLD_IDC, UNIT_A,    CH1,  6, 2,  100 },
    { FLD_PDC, UNIT_W,    CH1, 10, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH1, 14, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH1, 22, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH2,  4, 2,   10 },
    { FLD_IDC, UNIT_A,    CH2,  8, 2,  100 },
    { FLD_PDC, UNIT_W,    CH2, 12, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH2, 18, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH2, 24, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH2, CALC_IRR_CH, CH2, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH2, CALC_MPDC_CH, CH2, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 26, 2,   10 },
    { FLD_F,   UNIT_HZ,   CH0, 28, 2,  100 },
    { FLD_PAC, UNIT_W,    CH0, 30, 2,   10 },
    { FLD_Q,   UNIT_VAR,  CH0, 32, 2,   10 }, // signed!
    { FLD_IAC, UNIT_A,    CH0, 34, 2,  100 },
    { FLD_PF,  UNIT_NONE, CH0, 36, 2, 1000 }, // signed!
    { FLD_T,   UNIT_C,    CH0, 38, 2,   10 }, // signed!
    { FLD_EVT, UNIT_NONE, CH0, 40, 2,    1 },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,  0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,  0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0, 0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0, 0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC }
};
#define HMS2CH_LIST_LEN      (sizeof(hms2chAssignment) / sizeof(byteAssign_t))
#define HMS2CH_PAYLOAD_LEN   42

//-------------------------------------
// HMS-1800, HMS-2000
//-------------------------------------
const byteAssign_t hms4chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2,   10 },
    { FLD_IDC, UNIT_A,    CH1,  6, 2,  100 },
    { FLD_PDC, UNIT_W,    CH1, 10, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH1, 14, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH1, 22, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH2,  4, 2,   10 },
    { FLD_IDC, UNIT_A,    CH2,  8, 2,  100 },
    { FLD_PDC, UNIT_W,    CH2, 12, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH2, 18, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH2, 24, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH2, CALC_IRR_CH, CH2, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH2, CALC_MPDC_CH, CH2, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH3, 26, 2,   10 },
    { FLD_IDC, UNIT_A,    CH3, 30, 2,  100 },
    { FLD_PDC, UNIT_W,    CH3, 34, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH3, 38, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH3, 46, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH3, CALC_IRR_CH, CH3, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH3, CALC_MPDC_CH, CH3, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH4, 28, 2,   10 },
    { FLD_IDC, UNIT_A,    CH4, 32, 2,  100 },
    { FLD_PDC, UNIT_W,    CH4, 36, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH4, 42, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH4, 48, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH4, CALC_IRR_CH, CH4, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH4, CALC_MPDC_CH, CH4, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 50, 2,   10 },
    { FLD_F,   UNIT_HZ,   CH0, 52, 2,  100 },
    { FLD_PAC, UNIT_W,    CH0, 54, 2,   10 },
    { FLD_Q,   UNIT_VAR,  CH0, 56, 2,   10 }, // signed!
    { FLD_IAC, UNIT_A,    CH0, 58, 2,  100 },
    { FLD_PF,  UNIT_NONE, CH0, 60, 2, 1000 }, // signed!
    { FLD_T,   UNIT_C,    CH0, 62, 2,   10 }, // signed!
    { FLD_EVT, UNIT_NONE, CH0, 64, 2,    1 },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,  UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC }
};
#define HMS4CH_LIST_LEN      (sizeof(hms4chAssignment) / sizeof(byteAssign_t))
#define HMS4CH_PAYLOAD_LEN   66

//-------------------------------------
// HMT-1800, HMT-2250
//-------------------------------------
const byteAssign_t hmt6chAssignment[] = {
    { FLD_UDC, UNIT_V,   CH1,  2, 2,   10 },
    { FLD_IDC, UNIT_A,   CH1,  4, 2,  100 },
    { FLD_PDC, UNIT_W,   CH1,  8, 2,   10 },
    { FLD_YT,  UNIT_KWH, CH1, 12, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH1, 20, 2,    1 },
    { FLD_IRR, UNIT_PCT, CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_MP,  UNIT_W,   CH1, CALC_MPDC_CH, CH1, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH2, CALC_UDC_CH, CH1, CMD_CALC },
    { FLD_IDC, UNIT_A,   CH2,  6, 2,  100 },
    { FLD_PDC, UNIT_W,   CH2, 10, 2,   10 },
    { FLD_YT,  UNIT_KWH, CH2, 16, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH2, 22, 2,    1 },
    { FLD_IRR, UNIT_PCT, CH2, CALC_IRR_CH, CH2, CMD_CALC },
    { FLD_MP,  UNIT_W,   CH2, CALC_MPDC_CH, CH2, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH3, 24, 2,   10 },
    { FLD_IDC, UNIT_A,   CH3, 26, 2,  100 },
    { FLD_PDC, UNIT_W,   CH3, 30, 2,   10 },
    { FLD_YT,  UNIT_KWH, CH3, 34, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH3, 42, 2,    1 },
    { FLD_IRR, UNIT_PCT, CH3, CALC_IRR_CH, CH3, CMD_CALC },
    { FLD_MP,  UNIT_W,   CH3, CALC_MPDC_CH, CH3, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH4, CALC_UDC_CH, CH3, CMD_CALC },
    { FLD_IDC, UNIT_A,   CH4, 28, 2,  100 },
    { FLD_PDC, UNIT_W,   CH4, 32, 2,   10 },
    { FLD_YT,  UNIT_KWH, CH4, 38, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH4, 44, 2,    1 },
    { FLD_IRR, UNIT_PCT, CH4, CALC_IRR_CH, CH4, CMD_CALC },
    { FLD_MP,  UNIT_W,   CH4, CALC_MPDC_CH, CH4, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH5, 46, 2,   10 },
    { FLD_IDC, UNIT_A,   CH5, 48, 2,  100 },
    { FLD_PDC, UNIT_W,   CH5, 52, 2,   10 },
    { FLD_YT,  UNIT_KWH, CH5, 56, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH5, 64, 2,    1 },
    { FLD_IRR, UNIT_PCT, CH5, CALC_IRR_CH, CH5, CMD_CALC },
    { FLD_MP,  UNIT_W,   CH5, CALC_MPDC_CH, CH5, CMD_CALC },

    { FLD_UDC, UNIT_V,   CH6, CALC_UDC_CH, CH5, CMD_CALC },
    { FLD_IDC, UNIT_A,   CH6, 50, 2,  100 },
    { FLD_PDC, UNIT_W,   CH6, 54, 2,   10 },
    { FLD_YT,  UNIT_KWH, CH6, 60, 4, 1000 },
    { FLD_YD,  UNIT_WH,  CH6, 66, 2,    1 },
    { FLD_IRR, UNIT_PCT, CH6, CALC_IRR_CH, CH6, CMD_CALC },
    { FLD_MP,  UNIT_W,   CH6, CALC_MPDC_CH, CH6, CMD_CALC },

    { FLD_UAC_1N,   UNIT_V,    CH0, 68, 2,   10 },
    { FLD_UAC_2N,   UNIT_V,    CH0, 70, 2,   10 },
    { FLD_UAC_3N,   UNIT_V,    CH0, 72, 2,   10 },
    { FLD_UAC_12,   UNIT_V,    CH0, 74, 2,   10 },
    { FLD_UAC_23,   UNIT_V,    CH0, 76, 2,   10 },
    { FLD_UAC_31,   UNIT_V,    CH0, 78, 2,   10 },
    { FLD_F,        UNIT_HZ,   CH0, 80, 2,  100 },
    { FLD_PAC,      UNIT_W,    CH0, 82, 2,   10 },
    { FLD_Q,        UNIT_VAR,  CH0, 84, 2,   10 },
    { FLD_IAC_1,    UNIT_A,    CH0, 86, 2,  100 },
    { FLD_IAC_2,    UNIT_A,    CH0, 88, 2,  100 },
    { FLD_IAC_3,    UNIT_A,    CH0, 90, 2,  100 },
    { FLD_PF,       UNIT_NONE, CH0, 92, 2, 1000 },
    { FLD_T,        UNIT_C,    CH0, 94, 2,   10 },
    { FLD_EVT,      UNIT_NONE, CH0, 96, 2,    1 },
    { FLD_YD,       UNIT_WH,   CH0, CALC_YD_CH0,   0, CMD_CALC },
    { FLD_YT,       UNIT_KWH,  CH0, CALC_YT_CH0,   0, CMD_CALC },
    { FLD_PDC,      UNIT_W,    CH0, CALC_PDC_CH0,  0, CMD_CALC },
    { FLD_EFF,      UNIT_PCT,  CH0, CALC_EFF_CH0,  0, CMD_CALC },
    { FLD_MP,       UNIT_W,    CH0, CALC_MPAC_CH0, 0, CMD_CALC }
};
#define HMT6CH_LIST_LEN      (sizeof(hmt6chAssignment) / sizeof(byteAssign_t))
#define HMT6CH_PAYLOAD_LEN   98

#endif /*__HMS_DEFINES_H__*/
