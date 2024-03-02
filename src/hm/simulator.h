//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __SIMULATOR_H__
#define __SIMULATOR_H__

#if defined(ENABLE_SIMULATOR)

#include "../defines.h"
#include "../utils/dbg.h"
#include "../utils/helper.h"
#include "hmSystem.h"
#include "hmInverter.h"
#include "Communication.h"

template<class HMSYSTEM>
class Simulator {
    public:
        void setup(HMSYSTEM *sys, uint32_t *ts, uint8_t ivId = 0) {
            mTimestamp = ts;
            mSys       = sys;
            mIvId      = ivId;
        }

        void addPayloadListener(payloadListenerType cb) {
            mCbPayload = cb;
        }

        void tick() {
            uint8_t cmd, len;
            uint8_t *payload;
            getPayload(&cmd, &payload, &len);

            Inverter<> *iv = mSys->getInverterByPos(mIvId);
            if (NULL == iv)
                return;

            DPRINT(DBG_INFO, F("add payload with cmd: 0x"));
            DBGHEXLN(cmd);

            if(GridOnProFilePara == cmd) {
                iv->addGridProfile(payload, len);
                return;
            }

            record_t<> *rec = iv->getRecordStruct(cmd);
            rec->ts = *mTimestamp;
            for (uint8_t i = 0; i < rec->length; i++) {
                iv->addValue(i, payload, rec);
                yield();
            }
            iv->doCalculations();

            if((nullptr != mCbPayload) && (GridOnProFilePara != cmd))
                (mCbPayload)(cmd, iv);
        }

    private:
        inline void getPayload(uint8_t *cmd, uint8_t *payload[], uint8_t *len) {
            switch(payloadCtrl) {
                default: *cmd = RealTimeRunData_Debug;    break;
                case 1:  *cmd = SystemConfigPara;         break;
                case 3:  *cmd = InverterDevInform_All;    break;
                case 5:  *cmd = InverterDevInform_Simple; break;
                case 7:  *cmd = GridOnProFilePara;        break;
            }

            if(payloadCtrl < 8)
                payloadCtrl++;

            switch(*cmd) {
                default:
                case RealTimeRunData_Debug:
                    *payload = plRealtime;
                    modifyAcPwr();
                    *len = 62;
                    break;
                case InverterDevInform_All:
                    *payload = plFirmware;
                    *len = 14;
                    break;
                case InverterDevInform_Simple:
                    *payload = plPart;
                    *len = 14;
                    break;
                case SystemConfigPara:
                    *payload = plLimit;
                    *len = 14;
                    break;
                case AlarmData:
                    *payload = plAlarm;
                    *len = 26;
                    break;
                case GridOnProFilePara:
                    *payload = plGrid;
                    *len = 70;
                    break;
            }
        }

        inline void modifyAcPwr() {
            uint16_t cur = (plRealtime[50] << 8) | plRealtime[51];
            uint16_t change = cur ^ 0xa332;
            if(0 == change)
                change = 140;
            else if(change > 200)
                change = (change % 200) + 1;

            if(cur > 7000)
                cur -= change;
            else
                cur += change;

            plRealtime[50] = (cur >> 8) & 0xff;
            plRealtime[51] = (cur     ) & 0xff;
        }

    private:
        HMSYSTEM *mSys = nullptr;
        uint8_t mIvId = 0;
        uint32_t *mTimestamp = nullptr;
        payloadListenerType mCbPayload = nullptr;
        uint8_t payloadCtrl = 0;

    private:
        uint8_t plRealtime[62] = {
            0x00, 0x01, 0x01, 0x24, 0x00, 0x22, 0x00, 0x23,
            0x00, 0x63, 0x00, 0x65, 0x00, 0x08, 0x5c, 0xbb,
            0x00, 0x09, 0x6f, 0x08, 0x00, 0x0c, 0x00, 0x0c,
            0x01, 0x1e, 0x00, 0x22, 0x00, 0x21, 0x00, 0x60,
            0x00, 0x5f, 0x00, 0x08, 0xdd, 0x84, 0x00, 0x09,
            0x13, 0x6f, 0x00, 0x0b, 0x00, 0x0b, 0x09, 0x27,
            0x13, 0x8c, 0x01, 0x75, 0x00, 0xc2, 0x00, 0x10,
            0x03, 0x77, 0x00, 0x61, 0x00, 0x02
        };

        uint8_t plPart[14] = {
            0x27, 0x1c, 0x10, 0x12, 0x10, 0x01, 0x01, 0x00,
            0x0a, 0x00, 0x20, 0x01, 0x00, 0x00
        };

        uint8_t plFirmware[14] = {
            0x00, 0x01, 0x80, 0x01, 0x00, 0x01, 0x60, 0x42,
            0x60, 0x42, 0x00, 0x00, 0x00, 0x00
        };

        uint8_t plLimit[14] = {
            0x00, 0x01, 0x03, 0xe8, 0x00, 0x00, 0x03, 0xe8,
            0xff, 0xff, 0xff, 0xff, 0x01, 0x68
        };

        uint8_t plGrid[70] = {
            0x0D, 0x00, 0x20, 0x00, 0x00, 0x08, 0x08, 0xFC,
            0x07, 0x30, 0x00, 0x01, 0x0A, 0x55, 0x00, 0x01,
            0x09, 0xE2, 0x10, 0x00, 0x13, 0x88, 0x12, 0x8E,
            0x00, 0x01, 0x14, 0x1E, 0x00, 0x01, 0x20, 0x00,
            0x00, 0x01, 0x30, 0x07, 0x01, 0x2C, 0x0A, 0x55,
            0x07, 0x30, 0x14, 0x1E, 0x12, 0x8E, 0x00, 0x32,
            0x00, 0x1E, 0x40, 0x00, 0x07, 0xD0, 0x00, 0x10,
            0x50, 0x00, 0x00, 0x01, 0x13, 0x9C, 0x01, 0x90,
            0x00, 0x10, 0x70, 0x00, 0x00, 0x01
        };

        uint8_t plAlarm[26] = {
            0x00, 0x01, 0x80, 0x01, 0x00, 0x01, 0x51, 0xc7,
            0x51, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02,
            0x00, 0x02, 0xa6, 0xc9, 0xa6, 0xc9, 0x65, 0x3e,
            0x47, 0x21
        };
};

#endif /*ENABLE_SIMULATOR*/
#endif /*__SIMULATOR_H__*/
