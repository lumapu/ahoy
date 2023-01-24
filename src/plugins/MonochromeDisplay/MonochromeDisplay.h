#ifndef __MONOCHROME_DISPLAY__
#define __MONOCHROME_DISPLAY__

#include <U8g2lib.h>
#include <Timezone.h>

#include "../../utils/helper.h"
#include "../../hm/hmSystem.h"

#define DISP_DEFAULT_TIMEOUT        60  // in seconds


static uint8_t bmp_logo[] PROGMEM = {
    B00000000, B00000000, // ................
    B11101100, B00110111, // ..##.######.##..
    B11101100, B00110111, // ..##.######.##..
    B11100000, B00000111, // .....######.....
    B11010000, B00001011, // ....#.####.#....
    B10011000, B00011001, // ...##..##..##...
    B10000000, B00000001, // .......##.......
    B00000000, B00000000, // ................
    B01111000, B00011110, // ...####..####...
    B11111100, B00111111, // ..############..
    B01111100, B00111110, // ..#####..#####..
    B00000000, B00000000, // ................
    B11111100, B00111111, // ..############..
    B11111110, B01111111, // .##############.
    B01111110, B01111110, // .######..######.
    B00000000, B00000000 // ................
};


static uint8_t bmp_arrow[] PROGMEM = {
    B00000000, B00011100, B00011100, B00001110, B00001110, B11111110, B01111111,
    B01110000, B01110000, B00110000, B00111000, B00011000, B01111111, B00111111,
    B00011110, B00001110, B00000110, B00000000, B00000000, B00000000, B00000000
};


static TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
static TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Tim

template<class HMSYSTEM>
class MonochromeDisplay {
    public:
        MonochromeDisplay() : mCE(CEST, CET) {}

        void setup(display_t *cfg, HMSYSTEM *sys, uint32_t *utcTs, uint8_t disp_reset, const char *version) {
            mCfg   = cfg;
            mSys   = sys;
            mUtcTs = utcTs;
            mNewPayload = false;
            mLoopCnt = 0;
            mTimeout = DISP_DEFAULT_TIMEOUT; // power off timeout (after inverters go offline)

            u8g2_cb_t *rot = (u8g2_cb_t *)((mCfg->rot180) ? U8G2_R2 : U8G2_R0);

            if(mCfg->type) {
                switch(mCfg->type) {
                    case 1:
                        mDisplay = new U8G2_PCD8544_84X48_F_4W_HW_SPI(rot, mCfg->pin0, mCfg->pin1, disp_reset);
                        break;
                    case 2:
                        mDisplay = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(rot, disp_reset, mCfg->pin0, mCfg->pin1);
                        break;
                    case 3:
                        mDisplay = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(rot, disp_reset, mCfg->pin0, mCfg->pin1);
                        break;
                }
                mDisplay->begin();

                mIsLarge = ((mDisplay->getWidth() > 120) && (mDisplay->getHeight() > 60));
                calcLineHeights();

                mDisplay->clearBuffer();
                mDisplay->setContrast(mCfg->contrast);
                printText("Ahoy!", 0, 35);
                printText("ahoydtu.de", 2, 20);
                printText(version, 3, 46);
                mDisplay->sendBuffer();
            }
        }

        void payloadEventListener(uint8_t cmd) {
            mNewPayload = true;
        }

        void tickerSecond() {
            if(mCfg->pwrSaveAtIvOffline) {
                if(mTimeout != 0)
                    mTimeout--;
            }
            if(mNewPayload || ((++mLoopCnt % 10) == 0)) {
                mNewPayload = false;
                mLoopCnt = 0;
                DataScreen();
            }
        }

    private:
        void DataScreen() {
            if (mCfg->type == 0)
                return;
            if(*mUtcTs == 0)
                return;

            float totalPower = 0;
            float totalYieldDay = 0;
            float totalYieldTotal = 0;

            bool isprod = false;

            Inverter<> *iv;
            record_t<> *rec;
            for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                iv = mSys->getInverterByPos(i);
                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if (iv == NULL)
                    continue;

                if (iv->isProducing(*mUtcTs))
                    isprod = true;

                totalPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
                totalYieldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
                totalYieldTotal += iv->getChannelFieldValue(CH0, FLD_YT, rec);
            }

            mDisplay->clearBuffer();

            // Logos
            //   pxMovement +x (0 - 6 px)
            uint8_t ex = (_mExtra % 7);
            if (isprod) {
                mDisplay->drawXBMP(5 + ex, 1, 8, 17, bmp_arrow);
                if (mCfg->logoEn)
                    mDisplay->drawXBMP(mDisplay->getWidth() - 24 + ex, 2, 16, 16, bmp_logo);
            }

            if ((totalPower > 0) && isprod) {
                mTimeout = DISP_DEFAULT_TIMEOUT;
                mDisplay->setPowerSave(false);
                mDisplay->setContrast(mCfg->contrast);
                if (totalPower > 999)
                    snprintf(_fmtText, sizeof(_fmtText), "%2.1f kW", (totalPower / 1000));
                else
                    snprintf(_fmtText, sizeof(_fmtText), "%3.0f W", totalPower);
                printText(_fmtText, 0, 20);
            } else {
                printText("offline", 0, 25);
                if(mCfg->pwrSaveAtIvOffline) {
                    if(mTimeout == 0)
                        mDisplay->setPowerSave(true);
                }
            }

            snprintf(_fmtText, sizeof(_fmtText), "today: %4.0f Wh", totalYieldDay);
            printText(_fmtText, 1);

            snprintf(_fmtText, sizeof(_fmtText), "total: %.1f kWh", totalYieldTotal);
            printText(_fmtText, 2);

            IPAddress ip = WiFi.localIP();
            if (!(_mExtra % 10) && (ip)) {
                printText(ip.toString().c_str(), 3);
            } else {
                // Get current time
                if(mIsLarge)
                    printText(ah::getDateTimeStr(mCE.toLocal(*mUtcTs)).c_str(), 3);
                else
                    printText(ah::getTimeStr(mCE.toLocal(*mUtcTs)).c_str(), 3);
            }
            mDisplay->sendBuffer();

            _mExtra++;
        }

        void calcLineHeights() {
            uint8_t yOff = 0;
            for(uint8_t i = 0; i < 4; i++) {
                setFont(i);
                yOff += (mDisplay->getMaxCharHeight() + 1);
                mLineOffsets[i] = yOff;
            }
        }

        inline void setFont(uint8_t line) {
            switch (line) {
                case 0:  mDisplay->setFont((mIsLarge) ? u8g2_font_ncenB14_tr : u8g2_font_lubBI14_tr); break;
                case 3:  mDisplay->setFont(u8g2_font_5x8_tr); break;
                default: mDisplay->setFont((mIsLarge) ? u8g2_font_ncenB10_tr : u8g2_font_5x8_tr); break;
            }
        }

        void printText(const char* text, uint8_t line, uint8_t dispX = 5) {
            if(!mIsLarge)
                dispX = (line == 0) ? 10 : 5;
            setFont(line);
            if(mCfg->pxShift)
                dispX += (_mExtra % 7); // add pixel movement
            mDisplay->drawStr(dispX, mLineOffsets[line], text);
        }

        // private member variables
        U8G2* mDisplay;

        uint8_t _mExtra;
        uint16_t mTimeout; // interval at which to power save (milliseconds)
        char _fmtText[32];

        bool mNewPayload;
        bool mIsLarge;
        uint8_t mLoopCnt;
        uint32_t *mUtcTs;
        uint8_t mLineOffsets[5];
        display_t *mCfg;
        HMSYSTEM *mSys;
        Timezone mCE;
};

#endif /*__MONOCHROME_DISPLAY__*/
