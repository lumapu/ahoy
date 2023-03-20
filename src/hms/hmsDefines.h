//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HMS_DEFINES_H__
#define __HMS_DEFINES_H__

#include "../hm/hmDefines.h"

//-------------------------------------
// HMS2000
//-------------------------------------
const byteAssign_t hms4chAssignment[] = {
    { FLD_UDC, UNIT_V,    CH1,  2, 2,   10 },
    { FLD_UDC, UNIT_V,    CH2,  4, 2,   10 },
    { FLD_IDC, UNIT_A,    CH1,  6, 2,  100 },
    { FLD_IDC, UNIT_A,    CH2,  8, 2,  100 },
    { FLD_PDC, UNIT_W,    CH1, 10, 2,   10 },
    { FLD_PDC, UNIT_W,    CH2, 12, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH1, 14, 4, 1000 },
    { FLD_YT,  UNIT_KWH,  CH2, 18, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH1, 22, 2,    1 },
    { FLD_YD,  UNIT_WH,   CH2, 24, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH1, CALC_IRR_CH, CH1, CMD_CALC },
    { FLD_IRR, UNIT_PCT,  CH2, CALC_IRR_CH, CH2, CMD_CALC },

    { FLD_UDC, UNIT_V,    CH3, 26, 2,   10 },
    { FLD_UDC, UNIT_V,    CH4, 28, 2,   10 },
    { FLD_IDC, UNIT_A,    CH3, 30, 2,  100 },
    { FLD_IDC, UNIT_A,    CH4, 32, 2,  100 },
    { FLD_PDC, UNIT_W,    CH3, 34, 2,   10 },
    { FLD_PDC, UNIT_W,    CH4, 36, 2,   10 },
    { FLD_YT,  UNIT_KWH,  CH3, 38, 4, 1000 },
    { FLD_YT,  UNIT_KWH,  CH4, 42, 4, 1000 },
    { FLD_YD,  UNIT_WH,   CH3, 46, 2,    1 },
    { FLD_YD,  UNIT_WH,   CH4, 48, 2,    1 },
    { FLD_IRR, UNIT_PCT,  CH3, CALC_IRR_CH, CH3, CMD_CALC },
    { FLD_IRR, UNIT_PCT,  CH4, CALC_IRR_CH, CH4, CMD_CALC },

    { FLD_UAC, UNIT_V,    CH0, 50, 2,   10 },
    { FLD_F,   UNIT_HZ,   CH0, 52, 2,  100 },
    { FLD_PAC, UNIT_W,    CH0, 54, 2,   10 },
    { FLD_Q,   UNIT_VAR,  CH0, 56, 2,   10 }, // signed!
    { FLD_IAC, UNIT_A,    CH0, 58, 2,  100 },
    { FLD_PF,  UNIT_NONE, CH0, 60, 2, 1000 }, // signed!
    { FLD_T,   UNIT_C,    CH0, 62, 2,   10 }, // signed!
    { FLD_EVT, UNIT_NONE, CH0, 64, 2,    1 },
    { FLD_YD,  UNIT_WH,   CH0, CALC_YD_CH0,  0, CMD_CALC },
    { FLD_YT,  UNIT_KWH,  CH0, CALC_YT_CH0,  0, CMD_CALC },
    { FLD_PDC, UNIT_W,    CH0, CALC_PDC_CH0, 0, CMD_CALC },
    { FLD_EFF, UNIT_PCT,  CH0, CALC_EFF_CH0, 0, CMD_CALC }
};
#define HMS4CH_LIST_LEN      (sizeof(hms4chAssignment) / sizeof(byteAssign_t))
#define HMS4CH_PAYLOAD_LEN   66

#endif /*__HMS_DEFINES_H__*/
