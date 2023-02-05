//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __PUB_SERIAL_H__
#define __PUB_SERIAL_H__

#include "../utils/dbg.h"
#include "../config/settings.h"
#include "../hm/hmSystem.h"

template<class HMSYSTEM>
class PubSerial {
    public:
        PubSerial() {}

        void setup(settings_t *cfg, HMSYSTEM *sys, uint32_t *utcTs) {
            mCfg = cfg;
            mSys = sys;
            mUtcTimestamp = utcTs;
        }

        void tick(void) {
            if (mCfg->serial.showIv) {
                char topic[32 + MAX_NAME_LENGTH], val[40];
                for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                    Inverter<> *iv = mSys->getInverterByPos(id);
                    if (NULL != iv) {
                        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                        if (iv->isAvailable(*mUtcTimestamp)) {
                            DPRINTLN(DBG_INFO, "Iv: " + String(id));
                            for (uint8_t i = 0; i < rec->length; i++) {
                                if (0.0f != iv->getValue(i, rec)) {
                                    snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/ch%d/%s", iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                                    snprintf(val, 40, "%.3f %s", iv->getValue(i, rec), iv->getUnit(i, rec));
                                    DPRINTLN(DBG_INFO, String(topic) + ": " + String(val));
                                }
                                yield();
                            }
                            DPRINTLN(DBG_INFO, "");
                        }
                    }
                }
            }
        }

    private:
        settings_t *mCfg;
        HMSYSTEM *mSys;
        uint32_t *mUtcTimestamp;
};


#endif /*__PUB_SERIAL_H__*/
