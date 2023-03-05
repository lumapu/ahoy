#ifndef __DISPLAY__
#define __DISPLAY__

#include <Timezone.h>
#include <U8g2lib.h>

#include "../../hm/hmSystem.h"
#include "../../utils/helper.h"
#include "Display_Mono.h"
#include "Display_ePaper.h"
#include "imagedata.h"

#define DISP_DEFAULT_TIMEOUT 60  // in seconds

template <class HMSYSTEM>
class Display {
   public:
    Display() {}

    void setup(display_t *cfg, HMSYSTEM *sys, uint32_t *utcTs, uint8_t disp_reset, const char *version) {
        mCfg = cfg;
        mSys = sys;
        mUtcTs = utcTs;
        mNewPayload = false;
        mLoopCnt = 0;
        mVersion = version;

        if (mCfg->type == 0) {
            return;
        } else if (1 < mCfg->type < 11) {
            switch (mCfg->rot) {
                case 0:
                    DisplayMono.disp_rotation = U8G2_R0;
                    break;
                case 1:
                    DisplayMono.disp_rotation = U8G2_R1;
                    break;
                case 2:
                    DisplayMono.disp_rotation = U8G2_R2;
                    break;
                case 3:
                    DisplayMono.disp_rotation = U8G2_R3;
                    break;
            }

            DisplayMono.enablePowerSafe = mCfg->pwrSaveAtIvOffline;
            DisplayMono.enableScreensaver = mCfg->pxShift;
            DisplayMono.contrast = mCfg->contrast;

            DisplayMono.init(mCfg->type, mCfg->disp_cs, mCfg->disp_dc, mCfg->disp_reset, mCfg->disp_busy, mCfg->disp_clk, mCfg->disp_data, mVersion);
        } else if (mCfg->type > 10) {
            DisplayEPaper.displayRotation = mCfg->rot;
            counterEPaper = 0;

            DisplayEPaper.init(mCfg->type, mCfg->disp_cs, mCfg->disp_dc, mCfg->disp_reset, mCfg->disp_busy, mCfg->disp_clk, mCfg->disp_data, mVersion);
        }
    }

    void payloadEventListener(uint8_t cmd) {
        mNewPayload = true;
    }

    void tickerSecond() {
        if (mNewPayload || ((++mLoopCnt % 10) == 0)) {
            mNewPayload = false;
            mLoopCnt = 0;
            DataScreen();
        }
    }

   private:
    void DataScreen() {
        if (mCfg->type == 0)
            return;
        if (*mUtcTs == 0)
            return;

        if ((millis() - _lastDisplayUpdate) > period) {
            float totalPower = 0;
            float totalYieldDay = 0;
            float totalYieldTotal = 0;

            uint8_t isprod = 0;

            Inverter<> *iv;
            record_t<> *rec;
            for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                iv = mSys->getInverterByPos(i);
                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if (iv == NULL)
                    continue;

                if (iv->isProducing(*mUtcTs))
                    uint8_t isprod = 0;

                totalPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
                totalYieldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
                totalYieldTotal += iv->getChannelFieldValue(CH0, FLD_YT, rec);
            }

            if (1 < mCfg->type < 11) {
                DisplayMono.loop(totalPower, totalYieldDay, totalYieldTotal, isprod);
            } else if (mCfg->type > 10) {
                DisplayEPaper.loop(totalPower, totalYieldDay, totalYieldTotal, isprod);
                counterEPaper++;
            }
            _lastDisplayUpdate = millis();
        }

        if (counterEPaper > 480) {
            DisplayEPaper.fullRefresh();
            counterEPaper = 0;
        }
    }

    // private member variables
    bool mNewPayload;
    uint8_t mLoopCnt;
    uint32_t *mUtcTs;
    const char *mVersion;
    display_t *mCfg;
    HMSYSTEM *mSys;
    uint16_t period = 10000;  // Achtung, max 65535
    uint16_t counterEPaper;
    uint32_t _lastDisplayUpdate = 0;
};

#endif /*__DISPLAY__*/
