#ifndef __HM_INVERTER_H__
#define __HM_INVERTER_H__

#include "hmDefines.h"

/**
 * For values which are of interest and not transmitted by the inverter can be
 * calculated automatically.
 * A list of functions can be linked to the assignment and will be executed
 * automatically. Their result does not differ from original read values.
 * The special command 0xff (CMDFF) must be used.
 */

// forward declaration of class
template <class RECORDTYPE=float>
class Inverter;


// prototypes
template<class T=float>
static T calcYieldTotalCh0(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcYieldDayCh0(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcUdcCh(Inverter<> *iv, uint8_t arg0);

template<class T=float>
using func_t = T (Inverter<> *, uint8_t);

template<class T=float>
struct calcFunc_t {
    uint8_t funcId; // unique id
    func_t<T>*  func;   // function pointer
} ;


// list of all available functions, mapped in hmDefines.h
template<class T=float>
const calcFunc_t<T> calcFunctions[] = {
    { CALC_YT_CH0, &calcYieldTotalCh0 },
    { CALC_YD_CH0, &calcYieldDayCh0   },
    { CALC_UDC_CH, &calcUdcCh         }
};


template <class RECORDTYPE>
class Inverter {
    public:
        uint8_t       id;       // unique id
        char          name[MAX_NAME_LENGTH]; // human readable name, eg. "HM-600.1"
        uint8_t       type;     // integer which refers to inverter type
        byteAssign_t* assign;   // type of inverter
        uint8_t       listLen;  // length of assignments
        serial_u      serial;   // serial number as on barcode
        serial_u      radioId;  // id converted to modbus
        uint8_t       channels; // number of PV channels (1-4)
        RECORDTYPE    *record;  // pointer for values

        Inverter() {

        }

        ~Inverter() {
            // TODO: cleanup
        }

        void init(void) {
            getAssignment();
            toRadioId();
            record = new RECORDTYPE[listLen];
            memset(name, 0, MAX_NAME_LENGTH);
            memset(record, 0, sizeof(RECORDTYPE) * listLen);
        }

        uint8_t getPosByChFld(uint8_t channel, uint8_t fieldId) {
            uint8_t pos = 0;
            for(; pos < listLen; pos++) {
                if((assign[pos].ch == channel) && (assign[pos].fieldId == fieldId))
                    break;
            }
            return (pos >= listLen) ? 0xff : pos;
        }

        const char *getFieldName(uint8_t pos) {
            return fields[assign[pos].fieldId];
        }

        const char *getUnit(uint8_t pos) {
            return units[assign[pos].unitId];
        }

        uint8_t getChannel(uint8_t pos) {
            return assign[pos].ch;
        }

        void addValue(uint8_t pos, uint8_t buf[]) {
            uint8_t ptr  = assign[pos].start;
            uint8_t end  = ptr + assign[pos].num;
            uint16_t div = assign[pos].div;

            if(CMD_CALC != div) {
                uint32_t val = 0;
                do {
                    val <<= 8;
                    val |= buf[ptr];
                } while(++ptr != end);

                record[pos] = (RECORDTYPE)(val) / (RECORDTYPE)(div);
            }
        }

        RECORDTYPE getValue(uint8_t pos) {
            return record[pos];
        }

        void doCalculations(void) {
            for(uint8_t i = 0; i < listLen; i++) {
                if(CMD_CALC == assign[i].div) {
                    record[i] = calcFunctions<RECORDTYPE>[assign[i].start].func(this, assign[i].num);
                }
            }
        }

    private:
        void toRadioId(void) {
            radioId.u64  = 0ULL;
            radioId.b[4] = serial.b[0];
            radioId.b[3] = serial.b[1];
            radioId.b[2] = serial.b[2];
            radioId.b[1] = serial.b[3];
            radioId.b[0] = 0x01;
        }

        void getAssignment(void) {
            if(INV_TYPE_1CH == type) {
                listLen  = (uint8_t)(HM1CH_LIST_LEN);
                assign   = (byteAssign_t*)hm1chAssignment;
                channels = 1;
            }
            else if(INV_TYPE_2CH == type) {
                listLen  = (uint8_t)(HM2CH_LIST_LEN);
                assign   = (byteAssign_t*)hm2chAssignment;
                channels = 2;
            }
            else if(INV_TYPE_4CH == type) {
                listLen  = (uint8_t)(HM4CH_LIST_LEN);
                assign   = (byteAssign_t*)hm4chAssignment;
                channels = 4;
            }
            else {
                listLen  = 0;
                channels = 0;
                assign   = NULL;
            }
        }
};


/**
 * To calculate values which are not transmitted by the unit there is a generic
 * list of functions which can be linked to the assignment.
 * The special command 0xff (CMDFF) must be used.
 */

template<class T=float>
static T calcYieldTotalCh0(Inverter<> *iv, uint8_t arg0) {
    if(NULL != iv) {
        T yield = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            uint8_t pos = iv->getPosByChFld(i, FLD_YT);
            yield += iv->getValue(pos);
        }
        return yield;
    }
    return 0.0;
}

template<class T=float>
static T calcYieldDayCh0(Inverter<> *iv, uint8_t arg0) {
    if(NULL != iv) {
        T yield = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            uint8_t pos = iv->getPosByChFld(i, FLD_YD);
            yield += iv->getValue(pos);
        }
        return yield;
    }
    return 0.0;
}

template<class T=float>
static T calcUdcCh(Inverter<> *iv, uint8_t arg0) {
    // arg0 = channel of source
    for(uint8_t i = 0; i < iv->listLen; i++) {
        if((FLD_UDC == iv->assign[i].fieldId) && (arg0 == iv->assign[i].ch)) {
            return iv->getValue(i);
        }
    }

    return 0.0;
}


#endif /*__HM_INVERTER_H__*/
