//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"

class DisplayMono84X48 : public DisplayMono {
    public:
        DisplayMono84X48() : DisplayMono() {
            mEnPowerSafe = true;
            mEnScreenSaver = true;
            mLuminance = 60;
            mExtra = 0;
            mDispY = 0;
            mTimeout = DISP_DEFAULT_TIMEOUT;  // interval at which to power save (milliseconds)
            mUtcTs = NULL;
            mType = 0;
        }

        void init(uint8_t type, uint8_t rotation, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t *utcTs, const char *version) {
            if((0 == type) || (type > 4))
                return;

            u8g2_cb_t *rot = (u8g2_cb_t *)((rotation != 0x00) ? U8G2_R2 : U8G2_R0);
            mType = type;
            mDisplay = new U8G2_PCD8544_84X48_F_4W_SW_SPI(rot, clock, data, cs, dc, reset);

            mUtcTs = utcTs;

            mDisplay->begin();
            calcLinePositions();

            mDisplay->clearBuffer();
            if (3 != mType)
                mDisplay->setContrast(mLuminance);
            printText("AHOY!", 0);
            printText("ahoydtu.de", 2);
            printText(version, 3);
            mDisplay->sendBuffer();
        }

        void config(bool enPowerSafe, bool enScreenSaver, uint8_t lum) {
            mEnPowerSafe = enPowerSafe;
            mEnScreenSaver = enScreenSaver;
            mLuminance = lum;
        }

        void loop(void) {
            if (mEnPowerSafe) {
                if (mTimeout != 0)
                        mTimeout--;
            }
        }

        void disp(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod) {
            mDisplay->clearBuffer();

            // set Contrast of the Display to raise the lifetime
            if (3 != mType)
                mDisplay->setContrast(mLuminance);

            if ((totalPower > 0) && (isprod > 0)) {
                mTimeout = DISP_DEFAULT_TIMEOUT;
                mDisplay->setPowerSave(false);

                if (totalPower > 999)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%2.2f kW", (totalPower / 1000));
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%3.0f W", totalPower);

                printText(mFmtText, 0);
            } else {
                printText("offline", 0);
                // check if it's time to enter power saving mode
                if (mTimeout == 0)
                    mDisplay->setPowerSave(mEnPowerSafe);
            }

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "today: %4.0f Wh", totalYieldDay);
            printText(mFmtText, 1);

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "total: %.1f kWh", totalYieldTotal);
            printText(mFmtText, 2);

            IPAddress ip = WiFi.localIP();
            if (!(mExtra % 10) && (ip))
                printText(ip.toString().c_str(), 3);
            else if (!(mExtra % 5)) {
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%d Inverter on", isprod);
                printText(mFmtText, 3);
            } else if (NULL != mUtcTs)
                printText(ah::getTimeStr(gTimezone.toLocal(*mUtcTs)).c_str(), 3);

            mDisplay->sendBuffer();

            mExtra = 1;
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
                    mDisplay->setFont(u8g2_font_logisoso16_tr);
                    break;
                case 3:
                    mDisplay->setFont(u8g2_font_5x8_tr);
                    break;
                default:
                    mDisplay->setFont(u8g2_font_5x8_tr);
                    break;
            }
        }

        void printText(const char *text, uint8_t line) {
            uint8_t dispX = (line == 0) ? 10 : 5;
            setFont(line);

            dispX += (mEnScreenSaver) ? (mExtra % 7) : 0;
            mDisplay->drawStr(dispX, mLineYOffsets[line], text);
        }
};
