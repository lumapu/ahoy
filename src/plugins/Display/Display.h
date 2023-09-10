#ifndef __DISPLAY__
#define __DISPLAY__

#include <Timezone.h>
#include <U8g2lib.h>

#include "../../hm/hmSystem.h"
#include "../../hm/hmRadio.h"
#include "../../utils/helper.h"
#include "Display_Mono.h"
#include "Display_Mono_128X32.h"
#include "Display_Mono_128X64.h"
#include "Display_Mono_84X48.h"
#include "Display_Mono_64X48.h"
#include "Display_ePaper.h"
#include "Display_data.h"

template <class HMSYSTEM, class HMRADIO>
class Display {
   public:
    Display() {
        mMono = NULL;
    }

    void setup(IApp *app, display_t *cfg, HMSYSTEM *sys, HMRADIO *radio, uint32_t *utcTs) {
        mApp = app;
        mHmRadio = radio;
        mCfg = cfg;
        mSys = sys;
        mUtcTs = utcTs;
        mNewPayload = false;
        mLoopCnt = 0;

        mDisplayData.version = app->getVersion();  // version never changes, so only set once

        switch (mCfg->type) {
            case 0: mMono = NULL; break;
            case 1: // fall-through
            case 2: mMono = new DisplayMono128X64(); break;
            case 3: mMono = new DisplayMono84X48(); break;
            case 4: mMono = new DisplayMono128X32(); break;
            case 5: mMono = new DisplayMono64X48(); break;
            case 6: mMono = new DisplayMono128X64(); break;
#if defined(ESP32)
            case 10:
                mMono = NULL;   // ePaper does not use this
                mRefreshCycle = 0;
                mEpaper.config(mCfg->rot, mCfg->pwrSaveAtIvOffline);
                mEpaper.init(mCfg->type, mCfg->disp_cs, mCfg->disp_dc, mCfg->disp_reset, mCfg->disp_busy, mCfg->disp_clk, mCfg->disp_data, mUtcTs, mDisplayData.version);
                break;
#endif

            default: mMono = NULL; break;
        }
        if(mMono) {
            mMono->config(mCfg->pwrSaveAtIvOffline, mCfg->pxShift, mCfg->contrast);
            mMono->init(mCfg->type, mCfg->rot, mCfg->disp_cs, mCfg->disp_dc, 0xff, mCfg->disp_clk, mCfg->disp_data, &mDisplayData);
        }
    }

    void payloadEventListener(uint8_t cmd) {
        mNewPayload = true;
    }

    void tickerSecond() {
        if (mMono != NULL)
            mMono->loop(mCfg->contrast);

        if (mNewPayload || (((++mLoopCnt) % 30) == 0)) {
            mNewPayload = false;
            mLoopCnt = 0;
            DataScreen();
        }
        #if defined(ESP32)
            mEpaper.tickerSecond();
        #endif
    }

   private:
    void DataScreen() {
        if (mCfg->type == 0)
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

            if (iv->isProducing())
                isprod++;

            totalPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
            totalYieldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
            totalYieldTotal += iv->getChannelFieldValue(CH0, FLD_YT, rec);
        }

        // prepare display data
        mDisplayData.isProducing = isprod;
        mDisplayData.totalPower = totalPower;
        mDisplayData.totalYieldDay = totalYieldDay;
        mDisplayData.totalYieldTotal = totalYieldTotal;
        mDisplayData.RadioSymbol = mHmRadio->isChipConnected();
        mDisplayData.wifiRSSI = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : SCHAR_MIN;
        mDisplayData.WiFiSymbol = (WiFi.status() == WL_CONNECTED);
        mDisplayData.ipAddress = WiFi.localIP();
        time_t utc= mApp->getTimestamp();
        if (year(utc) > 2020)
            mDisplayData.utcTs = utc;
        else
            mDisplayData.utcTs = 0;

        if (mMono ) {
            mMono->disp();
        }
#if defined(ESP32)
        else if (mCfg->type == 10) {
            mEpaper.loop(totalPower, totalYieldDay, totalYieldTotal, isprod);
            mRefreshCycle++;
        }

        if (mRefreshCycle > 480) {
            mEpaper.fullRefresh();
            mRefreshCycle = 0;
        }
#endif
    }

    // private member variables
    IApp *mApp;
    DisplayData mDisplayData;
    bool mNewPayload;
    uint8_t mLoopCnt;
    uint32_t *mUtcTs;
    display_t *mCfg;
    HMSYSTEM *mSys;
    HMRADIO *mHmRadio;
    uint16_t mRefreshCycle;

#if defined(ESP32)
    DisplayEPaper mEpaper;
#endif
    DisplayMono *mMono;
};

#endif /*__DISPLAY__*/
