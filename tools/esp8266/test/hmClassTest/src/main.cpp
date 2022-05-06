#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>


//-----------------------------------------------------------------------------
#define MAX_NUM_INVERTERS       3
#define MAX_NAME_LENGTH         16
#define NDEBUG
#define NO_RADIO

#include "../../../hmDefines.h"
#include "../../../hmInverter.h"
#include "../../../hmSystem.h"



//-----------------------------------------------------------------------------
typedef int RadioType;
typedef int BufferType;
typedef Inverter<float> InverterType;
typedef HmSystem<RadioType, BufferType, MAX_NUM_INVERTERS, InverterType> HmSystemType;


//-----------------------------------------------------------------------------
void valToBuf(InverterType *iv, uint8_t fld, uint8_t ch, float val, uint8_t bufPos);


//-----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    HmSystemType sys;
    InverterType *iv0, *iv1;
    uint8_t buf[30] = { 0xcc };

    iv0 = sys.addInverter("HM1200", 0x1122334455ULL, INV_TYPE_HM1200);
    iv1 = sys.addInverter("HM600",  0x1234567891ULL, INV_TYPE_HM600);

    valToBuf(iv0, FLD_UDC, CH1, 29.5,  3);
    valToBuf(iv0, FLD_UDC, CH3, 30.6,  9);
    valToBuf(iv0, FLD_YD,  CH1, 1234,  5);
    valToBuf(iv0, FLD_YD,  CH2, 1199,  7);
    valToBuf(iv0, FLD_YD,  CH3,  899, 11);
    valToBuf(iv0, FLD_YD,  CH4,  932, 13);
    valToBuf(iv0, FLD_YT,  CH1, 40.123, 13);
    valToBuf(iv0, FLD_YT,  CH2, 57.231,  1);
    valToBuf(iv0, FLD_YT,  CH3, 59.372,  3);
    valToBuf(iv0, FLD_YT,  CH4, 43.966,  7);

    iv0->doCalculations();
    for(uint8_t i = 0; i < iv0->listLen; i ++) {
        float val = iv0->getValue(i);
        if(0.0 != val) {
            printf("%10s [CH%d] = %.3f %s\n", iv0->getFieldName(i), iv0->getChannel(i), val, iv0->getUnit(i));
        }
    }

    return 0;
}


//-----------------------------------------------------------------------------
void valToBuf(InverterType *iv, uint8_t fld, uint8_t ch, float val, uint8_t bufPos) {
    uint8_t buf[30] = { 0xcc };
    uint8_t len;
    uint16_t factor;

    switch(fld) {
        default:     len = 2; break;
        case FLD_YT: len = 4; break;
    }

    switch(fld) {
        case FLD_YD:  factor = 1;    break;
        case FLD_UDC:
        case FLD_PDC:
        case FLD_UAC:
        case FLD_PAC:
        case FLD_PCT:
        case FLD_T:   factor = 10;   break;
        case FLD_IDC:
        case FLD_IAC:
        case FLD_F:   factor = 100;  break;
        default:      factor = 1000; break;
    }

    uint8_t *p = &buf[bufPos];

    uint32_t intval = (uint32_t)(val * factor);
    if(2 == len) {
        p[0] = (intval >> 8) & 0xff;
        p[1] = (intval     ) & 0xff;
    }
    else {
        p[0] = (intval >> 24) & 0xff;
        p[1] = (intval >> 16) & 0xff;
        p[2] = (intval >>  8) & 0xff;
        p[3] = (intval      ) & 0xff;
    }
    iv->addValue(iv->getPosByChFld(ch, fld), buf);
}
