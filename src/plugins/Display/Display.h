#ifndef __DISPLAY__
#define __DISPLAY__

#include <Timezone.h>
#include <U8g2lib.h>

#include "../../hm/hmSystem.h"
#include "../../utils/helper.h"
#include "Display_Mono.h"
#include "Display_ePaper.h"

template <class HMSYSTEM>
class Display {
   public:
        Display() {}

        void setup(display_t *cfg, HMSYSTEM *sys, uint32_t *utcTs, const char *version) {
            mCfg = cfg;
            mSys = sys;
            mUtcTs = utcTs;
            mNewPayload = false;
            mLoopCnt = 0;
            mVersion = version;

            if (mCfg->type == 0)
                return;

            if ((0 < mCfg->type) && (mCfg->type < 10)) {
                mMono.config(mCfg->pwrSaveAtIvOffline, mCfg->pxShift, mCfg->contrast);
                mMono.init(mCfg->type, mCfg->rot, mCfg->disp_cs, mCfg->disp_dc, 0xff, mCfg->disp_clk, mCfg->disp_data, mUtcTs, mVersion);
            } else if (mCfg->type >= 10) {
                #if defined(ESP32)
                mRefreshCycle = 0;
                mEpaper.config(mCfg->rot);
                mEpaper.init(mCfg->type, mCfg->disp_cs, mCfg->disp_dc, mCfg->disp_reset, mCfg->disp_busy, mCfg->disp_clk, mCfg->disp_data, mUtcTs, mVersion);
                #endif
            }
        }

        void payloadEventListener(uint8_t cmd) {
            mNewPayload = true;
        }

        void tickerSecond() {
            mMono.loop();
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
                    isprod++;

                totalPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
                totalYieldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
                totalYieldTotal += iv->getChannelFieldValue(CH0, FLD_YT, rec);
            }

            if ((0 < mCfg->type) && (mCfg->type < 10)) {
                mMono.disp(totalPower, totalYieldDay, totalYieldTotal, isprod);
            } else if (mCfg->type >= 10) {
                #if defined(ESP32)
                mEpaper.loop(totalPower, totalYieldDay, totalYieldTotal, isprod);
                mRefreshCycle++;
                #endif
            }

            #if defined(ESP32)
            if (mRefreshCycle > 480) {
                mEpaper.fullRefresh();
                mRefreshCycle = 0;
            }
            #endif
        }

        // private member variables
        bool mNewPayload;
        uint8_t mLoopCnt;
        uint32_t *mUtcTs;
        const char *mVersion;
        display_t *mCfg;
        HMSYSTEM *mSys;
        uint16_t mRefreshCycle;

        #if defined(ESP32)
        DisplayEPaper mEpaper;
        #endif
        DisplayMono mMono;
};

#endif /*__DISPLAY__*/
