#ifndef __DISPLAY__
#define __DISPLAY__

#if defined(PLUGIN_DISPLAY)

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

template <class HMSYSTEM, class RADIO>
class Display {
   public:
    Display() {
        mMono = NULL;
    }

    void setup(IApp *app, display_t *cfg, HMSYSTEM *sys, RADIO *hmradio, RADIO *hmsradio, uint32_t *utcTs) {
        mApp = app;
        mHmRadio  = hmradio;
        mHmsRadio = hmsradio;
        mCfg = cfg;
        mSys = sys;
        mUtcTs = utcTs;
        mNewPayload = false;
        mLoopCnt = 0;

        mDisplayData.version = app->getVersion();  // version never changes, so only set once

        switch (mCfg->type) {
            case DISP_TYPE_T0_NONE:             mMono = NULL; break;                    // None
            case DISP_TYPE_T1_SSD1306_128X64:   mMono = new DisplayMono128X64(); break; // SSD1306_128X64 (0.96", 1.54")
            case DISP_TYPE_T2_SH1106_128X64:    mMono = new DisplayMono128X64(); break; // SH1106_128X64 (1.3")
            case DISP_TYPE_T3_PCD8544_84X48:    mMono = new DisplayMono84X48();  break; // PCD8544_84X48 (1.6" - Nokia 5110)
            case DISP_TYPE_T4_SSD1306_128X32:   mMono = new DisplayMono128X32(); break; // SSD1306_128X32 (0.91")
            case DISP_TYPE_T5_SSD1306_64X48:    mMono = new DisplayMono64X48();  break; // SSD1306_64X48 (0.66" - Wemos OLED Shield)
            case DISP_TYPE_T6_SSD1309_128X64:   mMono = new DisplayMono128X64(); break; // SSD1309_128X64 (2.42")
#if defined(ESP32) && !defined(ETHERNET)
            case DISP_TYPE_T10_EPAPER:
                mMono = NULL;   // ePaper does not use this
                mRefreshCycle = 0;
                mEpaper.config(mCfg->rot, mCfg->pwrSaveAtIvOffline);
                mEpaper.init(mCfg->type, mCfg->disp_cs, mCfg->disp_dc, mCfg->disp_reset, mCfg->disp_busy, mCfg->disp_clk, mCfg->disp_data, mUtcTs, mDisplayData.version);
                break;
#endif
            default: mMono = NULL; break;
        }
        if(mMono) {
            mMono->config(mCfg);
            mMono->init(&mDisplayData);
        }

        // setup PIR pin for motion sensor
#ifdef ESP32
        if ((mCfg->screenSaver == 2) && (mCfg->pirPin != DEF_PIN_OFF))
            pinMode(mCfg->pirPin, INPUT);
#endif
#ifdef ESP8266
        if ((mCfg->screenSaver == 2) && (mCfg->pirPin != DEF_PIN_OFF) && (mCfg->pirPin != A0))
            pinMode(mCfg->pirPin, INPUT);
#endif

    }

    void payloadEventListener(uint8_t cmd) {
        mNewPayload = true;
    }

    void tickerSecond() {
        bool request_refresh = false;

        if (mMono != NULL)
            request_refresh = mMono->loop(motionSensorActive());

        if (mNewPayload || (((++mLoopCnt) % 5) == 0) || request_refresh) {
            DataScreen();
            mNewPayload = false;
            mLoopCnt = 0;
        }
        #if defined(ESP32) && !defined(ETHERNET)
            mEpaper.tickerSecond();
        #endif
    }

    private:
    void DataScreen() {
        if (DISP_TYPE_T0_NONE == mCfg->type)
            return;

        float totalPower = 0.0;
        float totalYieldDay = 0.0;
        float totalYieldTotal = 0.0;

        uint8_t nrprod = 0;
        uint8_t nrsleep = 0;
        int8_t  minQAllInv = 4;

        Inverter<> *iv;
        record_t<> *rec;
        bool allOff = true;
        uint8_t nInv = mSys->getNumInverters();
        for (uint8_t i = 0; i < nInv; i++) {
            iv = mSys->getInverterByPos(i);
            if (iv == NULL)
                continue;

            if (iv->isProducing())  // also updates inverter state engine
                nrprod++;
            else
                nrsleep++;

            rec = iv->getRecordStruct(RealTimeRunData_Debug);

            if (iv->isAvailable()) {  // consider only radio quality of inverters still communicating
                int8_t maxQInv = -6;
                for(uint8_t ch = 0; ch < RF_MAX_CHANNEL_ID; ch++) {
                    int8_t q = iv->heuristics.txRfQuality[ch];
                    if (q > maxQInv)
                        maxQInv = q;
                }
                if (maxQInv < minQAllInv)
                    minQAllInv = maxQInv;

                totalPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec); // add only FLD_PAC from inverters still communicating
                allOff = false;
            }

            totalYieldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
            totalYieldTotal += iv->getChannelFieldValue(CH0, FLD_YT, rec);
        }

        if (allOff)
             minQAllInv = -6;

        // prepare display data
        mDisplayData.nrProducing = nrprod;
        mDisplayData.nrSleeping = nrsleep;
        mDisplayData.totalPower = totalPower;
        mDisplayData.totalYieldDay = totalYieldDay;
        mDisplayData.totalYieldTotal = totalYieldTotal;
        bool nrf_en = mApp->getNrfEnabled();
        bool nrf_ok = nrf_en && mHmRadio->isChipConnected();
        #if defined(ESP32)
        bool cmt_en = mApp->getCmtEnabled();
        bool cmt_ok = cmt_en && mHmsRadio->isChipConnected();
        #else
        bool cmt_en = false;
        bool cmt_ok = false;
        #endif
        mDisplayData.RadioSymbol = (nrf_ok && !cmt_en) || (cmt_ok && !nrf_en) || (nrf_ok && cmt_ok);
        mDisplayData.WifiSymbol = (WiFi.status() == WL_CONNECTED);
        mDisplayData.MQTTSymbol = mApp->getMqttIsConnected();
        mDisplayData.RadioRSSI = ivQuality2RadioRSSI(minQAllInv); // Workaround as NRF24 has no RSSI. Approximation by quality levels from heuristic function
        mDisplayData.WifiRSSI = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : SCHAR_MIN;
        mDisplayData.ipAddress = WiFi.localIP();

        // provide localized times to display classes
        time_t utc= mApp->getTimestamp();
        if (year(utc) > 2020)
            mDisplayData.utcTs = gTimezone.toLocal(utc);
        else
            mDisplayData.utcTs = 0;
        mDisplayData.pGraphStartTime = gTimezone.toLocal(mApp->getSunrise());
        mDisplayData.pGraphEndTime = gTimezone.toLocal(mApp->getSunset());

        if (mMono ) {
            mMono->disp();
        }
#if defined(ESP32) && !defined(ETHERNET)
        else if (DISP_TYPE_T10_EPAPER == mCfg->type) {
            mEpaper.loop((totalPower), totalYieldDay, totalYieldTotal, nrprod);
            mRefreshCycle++;
        }

        if (mRefreshCycle > 480) {
            mEpaper.fullRefresh();
            mRefreshCycle = 0;
        }
#endif
    }

    bool motionSensorActive() {
        if ((mCfg->screenSaver == 2) && (mCfg->pirPin != DEF_PIN_OFF)) {
#if defined(ESP8266)
            if (mCfg->pirPin == A0)
                return((analogRead(A0) >= 512));
            else
                return(digitalRead(mCfg->pirPin));
#elif defined(ESP32)
            return(digitalRead(mCfg->pirPin));
#endif
        }
        else
            return(false);
    }

    // approximate RSSI in dB by invQuality levels from heuristic function (very unscientific but better than nothing :-) )
    int8_t ivQuality2RadioRSSI(int8_t invQuality) {
        int8_t pseudoRSSIdB;
        switch(invQuality) {
            case  4: pseudoRSSIdB = -55; break;
            case  3:
            case  2:
            case  1: pseudoRSSIdB = -65; break;
            case  0:
            case -1:
            case -2: pseudoRSSIdB = -75; break;
            case -3:
            case -4:
            case -5: pseudoRSSIdB = -85; break;
            case -6:
            default:  pseudoRSSIdB = -95; break;
        }
        return (pseudoRSSIdB);
    }

    // private member variables
    IApp *mApp;
    DisplayData mDisplayData;
    bool mNewPayload;
    uint8_t mLoopCnt;
    uint32_t *mUtcTs;
    display_t *mCfg;
    HMSYSTEM *mSys;
    RADIO *mHmRadio;
    RADIO *mHmsRadio;
    uint16_t mRefreshCycle;

#if defined(ESP32) && !defined(ETHERNET)
    DisplayEPaper mEpaper;
#endif
    DisplayMono *mMono;
};

#endif /*PLUGIN_DISPLAY*/

#endif /*__DISPLAY__*/
