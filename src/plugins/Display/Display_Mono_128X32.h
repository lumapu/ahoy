//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"

class DisplayMono128X32 : public DisplayMono {
    public:
        DisplayMono128X32() : DisplayMono() {
            mExtra = 0;
        }

        void config(display_t *cfg) override {
            mCfg = cfg;
        }

        void init(DisplayData *displayData) override {
            u8g2_cb_t *rot = (u8g2_cb_t *)((mCfg->rot != 0x00) ? U8G2_R2 : U8G2_R0);
            monoInit(new U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C(rot, 0xff, mCfg->disp_clk, mCfg->disp_data), displayData);
            calcLinePositions();
            printText("Ahoy!", 0);
            printText("ahoydtu.de", 2);
            printText(mDisplayData->version, 3);
            mDisplay->sendBuffer();
        }

        void disp(void)  override {
            mDisplay->clearBuffer();

            // calculate current pixelshift for pixelshift screensaver
            calcPixelShift(pixelShiftRange);

            if ((mDisplayData->totalPower > 0) && (mDisplayData->nrProducing > 0)) {
                if (mDisplayData->totalPower > 999)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%2.2f kW", (mDisplayData->totalPower / 1000));
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%3.0f W", mDisplayData->totalPower);

                printText(mFmtText, 0);
            } else {
                printText(STR_OFFLINE, 0);
            }

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%s: %4.0f Wh", STR_TODAY, mDisplayData->totalYieldDay);
            printText(mFmtText, 1);

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%s: %.1f kWh", STR_TOTAL, mDisplayData->totalYieldTotal);
            printText(mFmtText, 2);

            IPAddress ip = WiFi.localIP();
            if (!(mExtra % 10) && (ip))
                printText(ip.toString().c_str(), 3);
            else if (!(mExtra % 5)) {
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%s: %d", STR_ACTIVE_INVERTERS, mDisplayData->nrProducing);
                printText(mFmtText, 3);
            } else if (0 != mDisplayData->utcTs)
                printText(ah::getTimeStr(mDisplayData->utcTs).c_str(), 3);

            mDisplay->sendBuffer();

            mExtra++;
        }

    private:
        const uint8_t pixelShiftRange = 7;  // number of pixels to shift from left to right (centered -> must be odd!)

        void calcLinePositions() {
            uint8_t yOff[] = {0, 0};
            for (uint8_t i = 0; i < 4; i++) {
                setFont(i);
                yOff[getColumn(i)] += (mDisplay->getMaxCharHeight());
                mLineYOffsets[i] = yOff[getColumn(i)];
                if (isTwoRowLine(i))
                    yOff[getColumn(i)] += mDisplay->getMaxCharHeight();
                yOff[getColumn(i)] += BOTTOM_MARGIN;
                mLineXOffsets[i] = (getColumn(i) == 1 ? 80 : 0);
            }
        }

        inline void setFont(uint8_t line) {
            switch (line) {
                case 0:
                    mDisplay->setFont(u8g2_font_9x15_tr);
                    break;
                case 3:
                    mDisplay->setFont(u8g2_font_tom_thumb_4x6_tr);
                    break;
                default:
                    mDisplay->setFont(u8g2_font_tom_thumb_4x6_tr);
                    break;
            }
        }

        inline uint8_t getColumn(uint8_t line) {
            if (line >= 1 && line <= 2)
                return 1;
            else
                return 0;
        }

        inline bool isTwoRowLine(uint8_t line) {
            return ((line >= 1) && (line <= 2));
        }

        void printText(const char *text, uint8_t line) {
            setFont(line);

            uint8_t dispX = mLineXOffsets[line] + (pixelShiftRange / 2) + mPixelshift;

            if (isTwoRowLine(line)) {
                String stringText = String(text);
                int space = stringText.indexOf(" ");
                mDisplay->drawStr(dispX, mLineYOffsets[line], stringText.substring(0, space).c_str());
                if (space > 0)
                    mDisplay->drawStr(dispX, mLineYOffsets[line] + mDisplay->getMaxCharHeight(), stringText.substring(space + 1).c_str());
            } else
                mDisplay->drawStr(dispX, mLineYOffsets[line], text);
        }
};
