#ifndef __HM_INVERTER_H__
#define __HM_INVERTER_H__

#include "hmDefines.h"

template <class RECORDTYPE=float>
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
            getAssignment();
            toRadioId();
            record = new RECORDTYPE[listLen];
            memset(record, 0, sizeof(RECORDTYPE) * listLen);
        }

        ~Inverter() {
            // TODO: cleanup
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

        uint8_t getCmdId(uint8_t pos) {
            return assign[pos].cmdId;
        }

        void addValue(uint8_t pos, uint8_t buf[]) {
            uint8_t ptr  = assign[pos].start;
            uint8_t end  = ptr + assign[pos].num;
            uint16_t div = assign[pos].div;

            uint32_t val = 0;
            do {
                val <<= 8;
                val |= buf[ptr];
            } while(++ptr != end);

            record[pos] = (RECORDTYPE)(val) / (RECORDTYPE)(div);
        }

        RECORDTYPE getValue(uint8_t pos) {
            return record[pos];
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
            if(INV_TYPE_HM600 == type) {
                listLen  = (uint8_t)(HM600_LIST_LEN);
                assign   = (byteAssign_t*)hm600assignment;
                channels = 2;
            }
            else if(INV_TYPE_HM1200 == type) {
                listLen  = (uint8_t)(HM1200_LIST_LEN);
                assign   = (byteAssign_t*)hm1200assignment;
                channels = 4;
            }
            else if(INV_TYPE_HM400 == type) {
                listLen  = (uint8_t)(HM400_LIST_LEN);
                assign   = (byteAssign_t*)hm400assignment;
                channels = 1;
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

typedef float (*func_t)(Inverter<> *);
typedef struct {
    uint8_t funcId; // unique id
    func_t  func;   // function pointer
} calcFunc_t;

static float calcYieldTotalCh0(Inverter<> *iv) {
    if(NULL != iv) {
        float yield[iv->channels];
        for(uint8_t i = 1; i <= iv->channels; i++) {
            uint8_t pos = iv->getPosByChFld(i, FLD_YT);
            //yield[i-1] = iv->getValue(iv)
        }
    }
    return 1.0;
}

static float calcYieldDayCh0(Inverter<> *iv) {
    return 1.0;
}

enum {CALC_YT_CH0 = 0, CALC_YD_CH0};

const calcFunc_t calcFunctions[] = {
    { CALC_YT_CH0, &calcYieldTotalCh0 },
    { CALC_YD_CH0, &calcYieldDayCh0   }
};


#endif /*__HM_INVERTER_H__*/
