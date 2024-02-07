//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HM_SYSTEM_H__
#define __HM_SYSTEM_H__

#include "../appInterface.h"
#include "hmInverter.h"
#include <functional>

template <uint8_t MAX_INVERTER=3, class INVERTERTYPE=Inverter<float>>
class HmSystem {
    public:
        HmSystem() {}

        void setup(uint32_t *timestamp, cfgInst_t *config, IApp *app) {
            mInverter[0].timestamp     = timestamp;
            mInverter[0].generalConfig = config;
            //mInverter[0].app           = app;
        }

        void addInverter(uint8_t id, std::function<void(Inverter<> *iv)> cb) {
            DPRINTLN(DBG_VERBOSE, F("hmSystem.h:addInverter"));
            INVERTERTYPE *iv = &mInverter[id];
            iv->id         = id;
            iv->config     = &mInverter[0].generalConfig->iv[id];
            DPRINT(DBG_VERBOSE, "SERIAL: " + String(iv->config->serial.b[5], HEX));
            DPRINTLN(DBG_VERBOSE, " " + String(iv->config->serial.b[4], HEX));
            if((iv->config->serial.b[5] == 0x11) || (iv->config->serial.b[5] == 0x10)) {
                switch(iv->config->serial.b[4]) {
                    case 0x24: // HMS-500
                    case 0x22:
                    case 0x21: iv->type = INV_TYPE_1CH;
                        break;

                    case 0x44: // HMS-1000
                    case 0x42:
                    case 0x41: iv->type = INV_TYPE_2CH;
                        break;

                    case 0x64: // HMS-2000
                    case 0x62:
                    case 0x61: iv->type = INV_TYPE_4CH;
                        break;

                    default:
                        DPRINTLN(DBG_ERROR, F("unknown inverter type"));
                        break;
                }

                if(iv->config->serial.b[5] == 0x11) {
                    if((iv->config->serial.b[4] & 0x0f) == 0x04) {
                        iv->ivGen = IV_HMS;
                        iv->ivRadioType = INV_RADIO_TYPE_CMT;
                    } else {
                        iv->ivGen = IV_HM;
                        iv->ivRadioType = INV_RADIO_TYPE_NRF;
                    }
                }
                else if((iv->config->serial.b[4] & 0x03) == 0x02) { // MI 3rd Gen -> same as HM
                    iv->ivGen = IV_HM;
                    iv->ivRadioType = INV_RADIO_TYPE_NRF;
                } else {  // MI 2nd Gen
                    iv->ivGen = IV_MI;
                    iv->ivRadioType = INV_RADIO_TYPE_NRF;
                }
            } else if(iv->config->serial.b[5] == 0x13) {
                    iv->ivGen = IV_HMT;
                    iv->type = INV_TYPE_6CH;
                    iv->ivRadioType = INV_RADIO_TYPE_CMT;
            } else if(iv->config->serial.u64 != 0ULL) {
                DPRINTLN(DBG_ERROR, F("inverter type can't be detected!"));
                return;
            } else
                iv->ivGen = IV_UNKNOWN;

            iv->init();
            if(IV_UNKNOWN == iv->ivGen)
                return; // serial is 0

            DPRINT(DBG_INFO, "added inverter ");
            if(iv->config->serial.b[5] == 0x11) {
                if((iv->config->serial.b[4] & 0x0f) == 0x04)
                    DBGPRINT("HMS");
                else
                    DBGPRINT("HM");
            } else if(iv->config->serial.b[5] == 0x13)
                DBGPRINT("HMT");
            else
                DBGPRINT(((iv->config->serial.b[4] & 0x03) == 0x01) ? " (2nd Gen) " : " (3rd Gen) ");

            DBGPRINTLN(String(iv->config->serial.u64, HEX));

            if((iv->config->serial.b[5] == 0x10) && ((iv->config->serial.b[4] & 0x03) == 0x01))
                DPRINTLN(DBG_WARN, F("MI Inverter, has some restrictions!"));

            #ifdef DYNAMIC_OFFSET
            iv->rxOffset = (iv->ivGen == IV_HM) ? 13 : 12; // effective 3 (or 2), but can easily be recognized as default setting
            #else
            //iv->rxOffset = ((iv->ivGen == IV_HM) && (iv->type == INV_TYPE_4CH)) ? 3 : 2;
            iv->rxOffset = (iv->ivGen == IV_HM) ? 3 : 2;
            #endif

            cb(iv);
        }

        INVERTERTYPE *findInverter(const uint8_t buf[]) {
            for(uint8_t i = 0; i < MAX_INVERTER; i++) {
                INVERTERTYPE *p = &mInverter[i];
                if((p->config->serial.b[3] == buf[0])
                    && (p->config->serial.b[2] == buf[1])
                    && (p->config->serial.b[1] == buf[2])
                    && (p->config->serial.b[0] == buf[3]))
                    return p;
            }
            return nullptr;
        }

        INVERTERTYPE *getInverterByPos(uint8_t pos, bool check = true) {
            DPRINTLN(DBG_VERBOSE, F("hmSystem.h:getInverterByPos"));
            if(pos >= MAX_INVERTER)
                return nullptr;
            else if((mInverter[pos].config->serial.u64 != 0ULL) || (false == check))
                return &mInverter[pos];
            else
                return nullptr;
        }

        uint8_t getNumInverters(void) {
            /*uint8_t num = 0;
            INVERTERTYPE *p;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                p = &mInverter[i];
                if(p->config->serial.u64 != 0ULL)
                    num++;
            }
            return num;*/
            return MAX_NUM_INVERTERS;
        }

    private:
        INVERTERTYPE mInverter[MAX_INVERTER];
};

#endif /*__HM_SYSTEM_H__*/
