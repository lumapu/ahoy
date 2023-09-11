//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"

class DisplayMono64X48 : public DisplayMono {
    public:
        DisplayMono64X48() : DisplayMono() {
            mEnPowerSave = true;
            mEnScreenSaver = false;
            mLuminance = 20;
            mExtra = 0;
            mDispY = 0;
            mTimeout = DISP_DEFAULT_TIMEOUT;  // interval at which to power save (milliseconds)
        }

        void config(bool enPowerSave, bool enScreenSaver, uint8_t lum) {
            mEnPowerSave = enPowerSave;
            mEnScreenSaver = enScreenSaver;
            mLuminance = lum;
        }

        void init(uint8_t type, uint8_t rotation, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, DisplayData *displayData) {
            u8g2_cb_t *rot = (u8g2_cb_t *)((rotation != 0x00) ? U8G2_R2 : U8G2_R0);
            // Wemos OLed Shield is not defined in u8 lib -> use nearest compatible
            monoInit(new U8G2_SSD1306_64X48_ER_F_HW_I2C(rot, reset, clock, data), type, displayData);
            calcLinePositions();
            printText("Ahoy!", 0);
            printText("ahoydtu.de", 1);
            printText(mDisplayData->version, 2);
            mDisplay->sendBuffer();
        }

        void loop(uint8_t lum) {
            if (mEnPowerSave) {
                if (mTimeout != 0)
                    mTimeout--;
            }

            if(mLuminance != lum) {
                mLuminance = lum;
                mDisplay->setContrast(mLuminance);
            }
        }

        void disp(void) {
            mDisplay->clearBuffer();

            // set Contrast of the Display to raise the lifetime
            mDisplay->setContrast(mLuminance);

            if ((mDisplayData->totalPower > 0) && (mDisplayData->nrProducing > 0)) {
                mTimeout = DISP_DEFAULT_TIMEOUT;
                mDisplay->setPowerSave(false);

                if (mDisplayData->totalPower > 999)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%2.2f kW", (mDisplayData->totalPower / 1000));
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%3.0f W", mDisplayData->totalPower);

                printText(mFmtText, 0);
            } else {
                printText("offline", 0);
                // check if it's time to enter power saving mode
                if (mTimeout == 0)
                    mDisplay->setPowerSave(mEnPowerSave);
            }

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "D: %4.0f Wh", mDisplayData->totalYieldDay);
            printText(mFmtText, 1);

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "T: %4.0f kWh", mDisplayData->totalYieldTotal);
            printText(mFmtText, 2);

            IPAddress ip = WiFi.localIP();
            if (!(mExtra % 10) && (ip))
                printText(ip.toString().c_str(), 3);
            else if (!(mExtra % 5)) {
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "active Inv: %d", mDisplayData->nrProducing);
                printText(mFmtText, 3);
            } else if (0 != mDisplayData->utcTs)
                printText(ah::getTimeStr(gTimezone.toLocal(mDisplayData->utcTs)).c_str(), 3);

            mDisplay->sendBuffer();

            mExtra++;
        }

    private:
        void calcLinePositions() {
            uint8_t yOff = 0;
            for (uint8_t i = 0; i < 4; i++) {
                setFont(i);
                yOff += (mDisplay->getMaxCharHeight());
                mLineYOffsets[i] = yOff;
            }
        }

        inline void setFont(uint8_t line) {
            switch (line) {
                case 0:
                    mDisplay->setFont(u8g2_font_fur11_tr);
                    break;
                case 1:
                case 2:
                    mDisplay->setFont(u8g2_font_6x10_tr);
                    break;
                case 3:
                    mDisplay->setFont(u8g2_font_4x6_tr);
                    break;
                case 4:
                    mDisplay->setFont(u8g2_font_4x6_tr);
                    break;
            }
        }

        void printText(const char *text, uint8_t line) {
            uint8_t dispX = 0; //small display, use all we have
            dispX += (mEnScreenSaver) ? (mExtra % 4) : 0;
            setFont(line);
            mDisplay->drawStr(dispX, mLineYOffsets[line], text);
        }
};
