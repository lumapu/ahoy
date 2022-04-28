#ifndef __HM1200_H
#define __HM1200_H

#define HM1200

const measureDef_t hm1200_measureDef[] = {
    { IDX_UDC,      UNIT_V,   CH1, CMD01, 14, BYTES2, DIV10   },
    { IDX_IDC,      UNIT_A,   CH1, CMD01, 16, BYTES2, DIV100  },
    { IDX_PDC,      UNIT_W,   CH1, CMD01, 20, BYTES2, DIV10   },
    { IDX_E_TAG,    UNIT_WH,  CH1, CMD02, 16, BYTES2, DIV1    },
    { IDX_E_TOTAL,  UNIT_KWH, CH1, CMD01, 24, BYTES4, DIV1000 },
    { IDX_UDC,      UNIT_V,   CH2, CMD02, 20, BYTES2, DIV10   },
    { IDX_IDC,      UNIT_A,   CH2, CMD01, 18, BYTES2, DIV100  },
    { IDX_PDC,      UNIT_W,   CH2, CMD01, 22, BYTES2, DIV10   },
    { IDX_E_TAG,    UNIT_WH,  CH2, CMD02, 18, BYTES2, DIV1    },
    { IDX_E_TOTAL,  UNIT_KWH, CH2, CMD02, 12, BYTES4, DIV1000 },
    { IDX_IDC,      UNIT_A,   CH3, CMD02, 22, BYTES2, DIV100  },
    { IDX_PDC,      UNIT_W,   CH3, CMD02, 26, BYTES2, DIV10   },
    { IDX_E_TAG,    UNIT_WH,  CH3, CMD03, 22, BYTES2, DIV1    },
    { IDX_E_TOTAL,  UNIT_KWH, CH3, CMD03, 14, BYTES4, DIV1000 },
    { IDX_IDC,      UNIT_A,   CH4, CMD02, 24, BYTES2, DIV100  },
    { IDX_PDC,      UNIT_W,   CH4, CMD03, 12, BYTES2, DIV10   },
    { IDX_E_TAG,    UNIT_WH,  CH4, CMD03, 24, BYTES2, DIV1    },
    { IDX_E_TOTAL,  UNIT_KWH, CH4, CMD03, 18, BYTES4, DIV1000 },
    { IDX_UAC,      UNIT_V,   CH0, CMD03, 26, BYTES2, DIV10   },
    { IDX_IPV,      UNIT_A,   CH0, CMD84, 18, BYTES2, DIV100  },
    { IDX_PAC,      UNIT_W,   CH0, CMD84, 14, BYTES2, DIV10   },
    { IDX_FREQ,     UNIT_HZ,  CH0, CMD84, 12, BYTES2, DIV100  },
    { IDX_PERCNT,   UNIT_PCT, CH0, CMD84, 20, BYTES2, DIV10   },
    { IDX_WR_TEMP,  UNIT_C,   CH0, CMD84, 22, BYTES2, DIV10   }
};

measureCalc_t hm1200_measureCalc[] = {};

#define HM1200_MEASURE_LIST_LEN  sizeof(hm1200_measureDef)/sizeof(measureDef_t)
#define HM1200_CALCED_LIST_LEN    0

#endif
