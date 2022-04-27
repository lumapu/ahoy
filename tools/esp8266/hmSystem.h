#ifndef __HM_SYSTEM_H__
#define __HM_SYSTEM_H__

#include "hmInverters.h"
#include "hmRadio.h"



template <class RADIO, class BUFFER, uint8_t MAX_INVERTER, class RECORDTYPE=float>
class HmSystem {
    public:
        typedef RADIO RadioType;
        RadioType Radio;
        typedef BUFFER BufferType;
        BufferType BufCtrl;

        HmSystem() {
            mNumInv = 0;
        }
        ~HmSystem() {
            // TODO: cleanup
        }

        void setup() {
            Radio.setup(&BufCtrl);
        }

        inverter_t *addInverter(const char *name, uint64_t serial, uint8_t type) {
            if(MAX_INVERTER <= mNumInv) {
                DPRINT("max number of inverters reached!");
                return NULL;
            }
            inverter_t *p = &mInverter[mNumInv];
            p->id         = mNumInv;
            p->serial.u64 = serial;
            p->type       = type;
            uint8_t len   = strlen(name);
            strncpy(p->name, name, (len > MAX_NAME_LENGTH) ? MAX_NAME_LENGTH : len);
            getAssignment(p);
            toRadioId(p);

            if(NULL == p->assign) {
                DPRINT("no assignment for type found!");
                return NULL;
            }
            else {
                mRecord[p->id] = new RECORDTYPE[p->listLen];
                memset(mRecord[p->id], 0, sizeof(RECORDTYPE) * p->listLen);
                mNumInv ++;
                return p;
            }
        }

        inverter_t *findInverter(uint8_t buf[]) {
            inverter_t *p;
            for(uint8_t i = 0; i < mNumInv; i++) {
                p = &mInverter[i];
                if((p->serial.b[3] == buf[0])
                    && (p->serial.b[2] == buf[1])
                    && (p->serial.b[1] == buf[2])
                    && (p->serial.b[0] == buf[3]))
                    return p;
            }
            return NULL;
        }

        inverter_t *getInverterByPos(uint8_t pos) {
            if(mInverter[pos].serial.u64 != 0ULL)
                return &mInverter[pos];
            else
                return NULL;
        }

        const char *getFieldName(inverter_t *p, uint8_t pos) {
            return fields[p->assign[pos].fieldId];
        }

        const char *getUnit(inverter_t *p, uint8_t pos) {
            return units[p->assign[pos].unitId];
        }

        uint64_t getSerial(inverter_t *p) {
            return p->serial.u64;
        }

        void updateSerial(inverter_t *p, uint64_t serial) {
            p->serial.u64 = serial;
        }

        uint8_t getChannel(inverter_t *p, uint8_t pos) {
            return p->assign[pos].ch;
        }

        uint8_t getCmdId(inverter_t *p, uint8_t pos) {
            return p->assign[pos].cmdId;
        }

        void addValue(inverter_t *p, uint8_t pos, uint8_t buf[]) {
            uint8_t ptr  = p->assign[pos].start;
            uint8_t end  = ptr + p->assign[pos].num;
            uint16_t div = p->assign[pos].div;

            uint32_t val = 0;
            do {
                val <<= 8;
                val |= buf[ptr];
            } while(++ptr != end);

            mRecord[p->id][pos] = (RECORDTYPE)(val) / (RECORDTYPE)(div);
        }

        RECORDTYPE getValue(inverter_t *p, uint8_t pos) {
            return mRecord[p->id][pos];
        }

        uint8_t getPosByChField(inverter_t *p, uint8_t channel, uint8_t fieldId) {
            uint8_t pos = 0;
            for(; pos < p->listLen; pos++) {
                if((p->assign[pos].ch == channel) && (p->assign[pos].fieldId == fieldId))
                    break;
            }
            return (pos >= p->listLen) ? 0xff : pos;
        }

        uint8_t getNumInverters(void) {
            return mNumInv;
        }

    private:
        void toRadioId(inverter_t *p) {
            p->radioId.u64  = 0ULL;
            p->radioId.b[4] = p->serial.b[0];
            p->radioId.b[3] = p->serial.b[1];
            p->radioId.b[2] = p->serial.b[2];
            p->radioId.b[1] = p->serial.b[3];
            p->radioId.b[0] = 0x01;
        }

        void getAssignment(inverter_t *p) {
            if(INV_TYPE_HM600 == p->type) {
                p->listLen = (uint8_t)(HM600_LIST_LEN);
                p->assign  = (byteAssign_t*)hm600assignment;
            }
            else if(INV_TYPE_HM1200 == p->type) {
                p->listLen = (uint8_t)(HM1200_LIST_LEN);
                p->assign  = (byteAssign_t*)hm1200assignment;
            }
            else if(INV_TYPE_HM400 == p->type) {
                p->listLen = (uint8_t)(HM400_LIST_LEN);
                p->assign  = (byteAssign_t*)hm400assignment;
            }
            else {
                p->listLen = 0;
                p->assign  = NULL;
            }
        }

        inverter_t mInverter[MAX_INVERTER];
        uint8_t mNumInv;
        RECORDTYPE *mRecord[MAX_INVERTER];
};

#endif /*__HM_SYSTEM_H__*/
