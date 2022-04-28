#ifndef __HM600_H
#define __HM600_H

#define HM600
#define HM700


float calcEheute (float *measure) { return measure[8] + measure[9]; }
float calcIpv    (float *measure) { return (measure[10] != 0 ? measure[12]/measure[10] : 0); }

const measureDef_t hm600_measureDef[] = {
  { IDX_UDC,    CH1, UNIT_V,  CMD01, 14, BYTES2, DIV10},
  { IDX_IDC,    CH1, UNIT_A,  CMD01, 16, BYTES2, DIV100},
  { IDX_PDC,    CH1, UNIT_W,  CMD01, 18, BYTES2, DIV10},
  { IDX_UDC,    CH2, UNIT_V,  CMD01, 20, BYTES2, DIV10},
  { IDX_IDC,    CH2, UNIT_A,  CMD01, 22, BYTES2, DIV100},
  { IDX_PDC,    CH2, UNIT_W,  CMD01, 24, BYTES2, DIV10},
  { IDX_E_WOCHE,CH0, UNIT_WH, CMD02, 12, BYTES2, DIV1},
  { IDX_E_TOTAL,CH0, UNIT_WH, CMD02, 14, BYTES4, DIV1},
  { IDX_E_TAG,  CH1, UNIT_WH, CMD02, 18, BYTES2, DIV1},
  { IDX_E_TAG,  CH2, UNIT_WH, CMD02, 20, BYTES2, DIV1},
  { IDX_UAC,    CH0, UNIT_V,  CMD02, 22, BYTES2, DIV10},
  { IDX_FREQ,   CH0, UNIT_HZ, CMD02, 24, BYTES2, DIV100},
  { IDX_PAC,    CH0, UNIT_W,  CMD02, 26, BYTES2, DIV10},
  { IDX_WR_TEMP,CH0, UNIT_C,  CMD83, 18, BYTES2, DIV10}     
};


measureCalc_t hm600_measureCalc[] = {
  { IDX_E_HEUTE,  UNIT_WH, DIV1,   &calcEheute},
  { IDX_IPV,      UNIT_A,  DIV100, &calcIpv}
};

#define HM600_MEASURE_LIST_LEN  sizeof(hm600_measureDef)/sizeof(measureDef_t)
#define HM600_CALCED_LIST_LEN   sizeof(hm600_measureCalc)/sizeof(measureCalc_t)

#endif
