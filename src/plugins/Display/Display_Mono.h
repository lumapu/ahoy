//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include <U8g2lib.h>
#define DISP_DEFAULT_TIMEOUT 60  // in seconds
#define DISP_FMT_TEXT_LEN 32
#define BOTTOM_MARGIN 5

#include "defines.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "../../utils/helper.h"
#include "Display_data.h"
#include "../../utils/dbg.h"
#include "../../utils/timemonitor.h"
#include "../../config/settings.h"

class DisplayMono {
    public:
        DisplayMono() {};

        virtual void init(DisplayData *displayData) = 0;
        virtual void config(display_t *cfg) = 0;
        virtual void disp(void) = 0;

        // Common loop function, manages display on/off functions for powersave and screensaver with motionsensor
        // can be overridden by subclasses
        virtual bool loop(bool motion) {

            bool dispConditions = (!mCfg->pwrSaveAtIvOffline || (mDisplayData->nrProducing > 0)) &&
                                         ((mCfg->screenSaver != 2) || motion); // screensaver 2 .. motionsensor

            if (mDisplayActive) {
                if (!dispConditions) {
                     if (mDisplayTime.isTimeout()) { // switch display off after timeout
                          mDisplayActive = false;
                          mDisplay->setPowerSave(true);
                     }
                }
                else
                     mDisplayTime.reStartTimeMonitor(); // keep display on
            }
            else {
                if (dispConditions) {
                     mDisplayActive = true;
                     mDisplayTime.reStartTimeMonitor(); // switch display on
                     mDisplay->setPowerSave(false);
                }
            }

            if(mLuminance != mCfg->contrast) {
                mLuminance = mCfg->contrast;
                mDisplay->setContrast(mLuminance);
            }

            return(monoMaintainDispSwitchState());  // return flag, if display content should be updated immediately
        }

    protected:
        enum class DispSwitchState {
            TEXT,
            GRAPH
        };

    protected:
        // Common initialization function to be called by subclasses
        void monoInit(U8G2* display, DisplayData *displayData) {
            mDisplay = display;
            mDisplayData = displayData;
            mDisplay->begin();
            mDisplay->setPowerSave(false);  // always start with display on
            mDisplay->setContrast(mLuminance);
            mDisplay->clearBuffer();
            mDispWidth = mDisplay->getDisplayWidth();
            mDispHeight = mDisplay->getDisplayHeight();
            mDispSwitchTime.stopTimeMonitor();
            if (100 == mCfg->graph_ratio)           // if graph ratio is 100% start in graph mode
                mDispSwitchState = DispSwitchState::GRAPH;
            else if (mCfg->graph_ratio != 0)
                mDispSwitchTime.startTimeMonitor(150 * (100 - mCfg->graph_ratio));  // start display mode change only if ratio is neither 0 nor 100
        }

        // pixelshift screensaver with wipe effect
        void calcPixelShift(int range) {
            int8_t mod = (millis() / 10000) % ((range >> 1) << 2);
            mPixelshift = (1 == mCfg->screenSaver) ? ((mod < range) ? mod - (range >> 1) : -(mod - range - (range >> 1) + 1)) : 0;
        }

    protected:
        enum class PowerGraphState {
            NO_TIME_SYNC,
            IN_PERIOD,
            WAIT_4_NEW_PERIOD,
            WAIT_4_RESTART
        };

        // initialize power graph and allocate data buffer based on pixel width
        void initPowerGraph(uint8_t width, uint8_t height) {
            DBGPRINTLN(F("---- Init Power Graph ----"));
            mPgWidth = width;
            mPgHeight = height;
            mPgData = new float[mPgWidth];
            mPgState = PowerGraphState::NO_TIME_SYNC;
            resetPowerGraph();
        }

        // add new value to power graph and maintain state engine for period times
        void addPowerGraphEntry(float val) {
            if (nullptr == mPgData)  // power graph not initialized
                return;

            bool store_entry = false;
            switch(mPgState) {
                case PowerGraphState::NO_TIME_SYNC:  
                    if ((mDisplayData->pGraphStartTime > 0) && (mDisplayData->pGraphEndTime > 0) &&                                         // wait until period data is available ...
                        (mDisplayData->utcTs >= mDisplayData->pGraphStartTime) && (mDisplayData->utcTs < mDisplayData->pGraphEndTime)) {    // and current time is in period
                        storeStartEndTimes();  // period was received -> store
                        store_entry = true;
                        mPgState = PowerGraphState::IN_PERIOD;
                    }
                    break;
                case PowerGraphState::IN_PERIOD:
                    if (mDisplayData->utcTs > mPgEndTime)                   // check if end of day is reached ...
                        mPgState = PowerGraphState::WAIT_4_NEW_PERIOD;      // then wait for new period setting
                    else
                        store_entry = true;                                
                    break;
                case PowerGraphState::WAIT_4_NEW_PERIOD:
                    if ((mPgStartTime != mDisplayData->pGraphStartTime) || (mPgEndTime != mDisplayData->pGraphEndTime)) { // wait until new time period was received ...
                        storeStartEndTimes();                                                                             // and store it for next period
                        mPgState = PowerGraphState::WAIT_4_RESTART;
                    }
                    break;
                case PowerGraphState::WAIT_4_RESTART:
                    if ((mDisplayData->utcTs >= mPgStartTime) && (mDisplayData->utcTs < mPgEndTime)) { // wait until current time is in period again ...
                        resetPowerGraph();                                                             // then reset power graph data
                        store_entry = true;
                        mPgState = PowerGraphState::IN_PERIOD;
                    }
                    break;
            }
            if (store_entry) {
                mPgLastPos = std::min((uint8_t) sss2PgPos(mDisplayData->utcTs - mPgStartTime), (uint8_t) (mPgWidth - 1));  // current datapoint based on seconds since start
                mPgData[mPgLastPos] = std::max(mPgData[mPgLastPos], val); // update current datapoint to maximum of all seen values
                mPgMaxPwr = std::max(mPgMaxPwr, val);  // update max value of stored data for scaling of y-axis
            }
        }

        // plot power graph to given display offset
        void plotPowerGraph(uint8_t xoff, uint8_t yoff) {
            if (nullptr == mPgData)  // power graph not initialized
                return;

            // draw axes
            mDisplay->drawLine(xoff, yoff, xoff,            yoff - mPgHeight);  // vertical axis
            mDisplay->drawLine(xoff, yoff, xoff + mPgWidth, yoff);              // horizontal axis

            // do not draw as long as time is not set correctly and no data was received
            if ((0 == mDisplayData->pGraphStartTime) || (0 == mDisplayData->pGraphEndTime) || (0 == mDisplayData->utcTs) || (mPgMaxPwr < 1) || (0 == mPgLastPos))
                return;

            // draw X scale
            tmElements_t tm;
            breakTime(mDisplayData->pGraphEndTime, tm);
            uint8_t endHourPg = tm.Hour;
            breakTime(mDisplayData->utcTs, tm);
            uint8_t endHour = std::min(endHourPg, tm.Hour);
            breakTime(mDisplayData->pGraphStartTime, tm);
            tm.Hour += 1;
            tm.Minute = 0;
            tm.Second = 0;
            for (; tm.Hour <= endHour; tm.Hour++) {
                uint8_t x_pos_screen = getPowerGraphXpos(sss2PgPos((uint32_t) makeTime(tm) - mDisplayData->pGraphStartTime)); // scale horizontal axis
                mDisplay->drawPixel(xoff + x_pos_screen, yoff - 1);
            }

            // draw Y scale
            uint16_t scale_y = 10;
            uint32_t maxpwr_int = static_cast<uint8_t>(std::round(mPgMaxPwr));
            if (maxpwr_int > 100)
                scale_y = 100;

            for (uint32_t i = scale_y; i <= maxpwr_int; i += scale_y) {
                uint8_t ypos = yoff - static_cast<uint8_t>(std::round(i * (float) mPgHeight / mPgMaxPwr)); // scale vertical axis
                mDisplay->drawPixel(xoff + 1, ypos);
            }

            // draw curve
            for (uint8_t i = 1; i <= mPgLastPos; i++) {
                mDisplay->drawLine(xoff + getPowerGraphXpos(i - 1), yoff - getPowerGraphValueYpos(i - 1),
                                   xoff + getPowerGraphXpos(i),     yoff - getPowerGraphValueYpos(i));
            }

            // print max power value
            mDisplay->setFont(u8g2_font_4x6_tr);
            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%dW", static_cast<uint16_t>(std::round(mPgMaxPwr)));
            mDisplay->drawStr(xoff + 3, yoff - mPgHeight + 5, mFmtText);
        }

    private:
        bool monoMaintainDispSwitchState(void) {
          bool change = false;
            switch(mDispSwitchState) {
                case DispSwitchState::TEXT:
                    if (mDispSwitchTime.isTimeout()) {
                        mDispSwitchState = DispSwitchState::GRAPH;
                        mDispSwitchTime.startTimeMonitor(150 * mCfg->graph_ratio);  // graph_ratio: 0-100 Gesamtperiode 15000 ms
                        change = true;
                    }
                    break;
                case DispSwitchState::GRAPH:
                    if (mDispSwitchTime.isTimeout()) {
                        mDispSwitchState = DispSwitchState::TEXT;
                        mDispSwitchTime.startTimeMonitor(150 * (100 - mCfg->graph_ratio));
                        change = true;
                    }
                    break;
            }
            return change;
        }

        // reset power graph 
        void resetPowerGraph() {
            if (mPgData != nullptr) {
                mPgMaxPwr = 0.0;
                mPgLastPos = 0;
                for (uint8_t i = 0; i < mPgWidth; i++) {
                    mPgData[i] = 0.0;
                }
            }
        }

        // store start and end times of current time period and calculate period length
        void storeStartEndTimes() {
            mPgStartTime = mDisplayData->pGraphStartTime;
            mPgEndTime = mDisplayData->pGraphEndTime;
            mPgPeriod = mDisplayData->pGraphEndTime - mDisplayData->pGraphStartTime;  // time period of power graph in sec for scaling of x-axis
        }

        // get power graph datapoint index, scaled to current time period, by seconds since start
        uint8_t sss2PgPos(uint seconds_since_start) { 
            if(mPgPeriod)                       
                return (seconds_since_start * (mPgWidth - 1) / mPgPeriod);
            else
                return 0;
        }

        // get X-position of power graph, scaled to lastpos, by according data point index
        uint8_t getPowerGraphXpos(uint8_t p) {     
            if ((p <= mPgLastPos) && (mPgLastPos > 0))
                return((p * (mPgWidth - 1)) / mPgLastPos);  // scaling of x-axis
            else
                return 0;
        }

        // get Y-position of power graph, scaled to maximum value, by according datapoint index
        uint8_t getPowerGraphValueYpos(uint8_t p) { 
            if ((p < mPgWidth) && (mPgMaxPwr > 0))
                return((mPgData[p] * (uint32_t) mPgHeight / mPgMaxPwr)); // scaling of data to graph height
            else
                return 0;
        }

    protected:
        display_t *mCfg;
        U8G2 *mDisplay;
        DisplayData *mDisplayData;
        DispSwitchState mDispSwitchState = DispSwitchState::TEXT;

        uint16_t mDispWidth;
        uint8_t mExtra;
        int8_t  mPixelshift=0;
        char mFmtText[DISP_FMT_TEXT_LEN];
        uint8_t mLineXOffsets[5] = {};
        uint8_t mLineYOffsets[5] = {};

        uint8_t mPgWidth = 0;

    private:
        float  *mPgData = nullptr;
        uint8_t mPgHeight = 0;
        float   mPgMaxPwr = 0.0;
        uint32_t mPgStartTime = 0;
        uint32_t mPgEndTime = 0;
        uint32_t mPgPeriod = 0;  // seconds
        uint8_t  mPgLastPos = 0;
        PowerGraphState mPgState = PowerGraphState::NO_TIME_SYNC;

        uint16_t mDispHeight;
        uint8_t mLuminance;

        TimeMonitor mDisplayTime = TimeMonitor(1000 * DISP_DEFAULT_TIMEOUT, true);
        TimeMonitor mDispSwitchTime = TimeMonitor();
        bool mDisplayActive = true;  // always start with display on
};

/* adapted 5x8 Font for low-res displays with symbols
Symbols:
     \x80 ... antenna
     \x81 ... WiFi
     \x82 ... suncurve
     \x83 ... sum/sigma
     \x84 ... antenna crossed
     \x85 ... WiFi crossed
     \x86 ... sun
     \x87 ... moon
     \x88 ... calendar/day
     \x89 ... MQTT               */
const uint8_t u8g2_font_5x8_symbols_ahoy[1049] U8G2_FONT_SECTION("u8g2_font_5x8_symbols_ahoy") =
  "j\0\3\2\4\4\3\4\5\10\10\0\377\6\377\6\0\1\61\2b\4\0 \5\0\304\11!\7a\306"
  "\212!\11\42\7\63\335\212\304\22#\16u\304\232R\222\14JePJI\2$\14u\304\252l\251m"
  "I\262E\0%\10S\315\212(\351\24&\13t\304\232(i\252\64%\1'\6\61\336\212\1(\7b"
  "\305\32\245))\11b\305\212(\251(\0*\13T\304\212(Q\206D\211\2+\12U\304\252\60\32\244"
  "\60\2,\7\63\275\32\245\4-\6\24\324\212!.\6\42\305\212!/\10d\304\272R[\6\60\14d"
  "\304\32%R\206DJ\24\0\61\10c\305\232Dj\31\62\13d\304\32%\312\22%\33\2\63\13d\304"
  "\212!\212D)Q\0\64\13d\304\252H\251\14Q\226\0\65\12d\304\212A\33\245D\1\66\13d\304"
  "\32%[\42)Q\0\67\13d\304\212!\213\262(\213\0\70\14d\304\32%J\224HJ\24\0\71\13"
  "d\304\32%\222\222-Q\0:\10R\305\212!\32\2;\10c\275\32\243R\2<\10c\305\252\244\224"
  "\25=\10\64\314\212!\34\2>\11c\305\212\254\224\224\0?\11c\305\232\246$M\0@\15\205\274*"
  ")\222\226DI\244\252\2A\12d\304\32%\222\206I\12B\14d\304\212%\32\222H\32\22\0C\12"
  "d\304\32%\322J\211\2D\12d\304\212%r\32\22\0E\12d\304\212A[\262l\10F\12d\304"
  "\212A[\262\32\0G\13d\304\32%\322\222)Q\0H\12d\304\212H\32&S\0I\10c\305\212"
  "%j\31J\12d\304\232)\253\224\42\0K\13d\304\212HI\244\244S\0L\10d\304\212\254\333\20"
  "M\12d\304\212h\70D\246\0N\12d\304\212h\31\226I\12O\12d\304\32%rJ\24\0P\13"
  "d\304\212%\222\206$\313\0Q\12t\274\32%\222\26\307\0R\13d\304\212%\222\206$\222\2S\14"
  "d\304\32%J\302$J\24\0T\11e\304\212A\12;\1U\11d\304\212\310S\242\0V\12d\304"
  "\212\310)\221\24\0W\12d\304\212\310\64\34\242\0X\13d\304\212HJ$%\222\2Y\12e\304\212"
  "LKja\11Z\12d\304\212!\213\332\206\0[\10c\305\212!j\32\134\10d\304\212,l\13]"
  "\10c\305\212\251i\10^\6#\345\232\6_\6\24\274\212!`\6\42\345\212(a\11D\304\232!\222"
  "\222\1b\13d\304\212,[\42iH\0c\7C\305\232)\23d\12d\304\272\312\20I\311\0e\11"
  "D\304\32%\31\262\1f\12d\304\252Ji\312\42\0g\12T\274\32%J\266D\1h\12d\304\212"
  ",[\42S\0i\10c\305\232P\252\14j\12s\275\252\64\212\224\12\0k\12d\304\212\254\64$\221"
  "\24l\10c\305\12\251\313\0m\12E\304\12\245EI\224\2n\10D\304\212%\62\5o\11D\304\32"
  "%\222\22\5p\12T\274\212%\32\222,\3q\11T\274\232!J\266\2r\11D\304\212$\261e\0"
  "s\10C\305\232![\0t\13d\304\232,\232\262$J\0u\10D\304\212\310\224\14v\10C\305\212"
  "\304R\1w\12E\304\212LI\224.\0x\11D\304\212(\221\224(y\13T\274\212HJ\206(Q"
  "\0z\11D\304\212!*\15\1{\12t\304*%L\304(\24|\6a\306\212\3}\13t\304\12\61"
  "\12\225\60\221\0~\10$\344\232DI\0\5\0\304\12\200\13u\274\212K\242T\266\260\4\201\14f"
  "D\233!\11#-\312!\11\202\15hD<\65\12\243,\214\302$\16\203\15w<\214C\22F\71\220"
  "\26\207A\204\13u\274\212\244\232t\313\241\10\205\17\206<\213\60\31\22\311\66D\245!\11\3\206\20\210"
  "<\254\342\20]\302(L\246C\30E\0\207\15wD\334X\25\267\341\20\15\21\0\210\16w<\214\203"
  "RQ\25I\212\324a\20\211\15f\304\213)\213\244,\222\222\245\0\0\0\0";


const uint8_t u8g2_font_ncenB08_symbols8_ahoy[173] U8G2_FONT_SECTION("u8g2_font_ncenB08_symbols8_ahoy") =
  "\13\0\3\2\4\4\1\2\5\10\11\0\0\10\0\10\0\0\0\0\0\0\224A\14\207\305\70H\321\222H"
  "k\334\6B\20\230\305\32\262\60\211\244\266\60T\243\34\326\0C\20\210\305S\243\60\312\302(\214\302("
  "L\342\0D\16\210\315\70(i\224#\71\20W\207\3E\15\207\305xI\206\323\232nIS\1F\25"
  "\230\305H\206\244\230$C\22\15Y\242\204j\224\205I$\5G\17\210\305*\16\321%\214\302d:\204"
  "Q\4H\14w\307\215Uq\33\16\321\20\1I\21\227\305\311\222aP\245H\221\244H\212\324a\20J"
  "\5\0\275\0K\5\0\315\0\0\0\0";

const uint8_t u8g2_font_ncenB10_symbols10_ahoy[217] U8G2_FONT_SECTION("u8g2_font_ncenB10_symbols10_ahoy") =
  "\13\0\3\2\4\4\2\2\5\13\13\0\0\13\0\13\0\0\0\0\0\0\300A\15\267\212q\220\42\251\322"
  "\266\306\275\1B\20\230\236\65da\22Ima\250F\71\254\1C\23\272\272\251\3Q\32\366Q\212\243"
  "\70\212\243\70\311\221\0D\20\271\252\361\242F:\242#: {\36\16\1E\17\267\212\221\264\3Q\35"
  "\332\321\34\316\341\14F\25\250\233\221\14I\61I\206$\252%J\250Fa\224%J\71G\30\273\312W"
  "\316r`T\262DJ\303\64L#%K\304\35\310\342,\3H\27\272\272\217\344P\16\351\210\16\354\300"
  "<\244C\70,\303 \16!\0I\24\271\252\241\34\336\1-\223\64-\323\62-\323\62\35x\10J\22"
  "\210\232\61Hi\64Di\64DI\226$KeiK\5\0\212\1\0\0\0";
