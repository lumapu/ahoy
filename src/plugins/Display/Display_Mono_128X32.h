//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"

class DisplayMono128X32 : public DisplayMono {
    public:
        DisplayMono128X32() : DisplayMono() {
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

            u8g2_cb_t *rot = (u8g2_cb_t *)((rotation != 0x00) ? U8G2_R2 : U8G2_R0);
            mType = type;
            mDisplay = new U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C(rot, reset, clock, data);

            mUtcTs = utcTs;

            mDisplay->begin();

            calcLinePositions();

            mDisplay->clearBuffer();
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

            mDispY = 0;
            mExtra++;
        }

    private:
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
                    mDisplay->setFont(u8g2_font_9x15_tf);
                    break;
                case 3:
                    mDisplay->setFont(u8g2_font_tom_thumb_4x6_tf);
                    break;
                default:
                    mDisplay->setFont(u8g2_font_tom_thumb_4x6_tf);
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

            uint8_t dispX = mLineXOffsets[line] + ((mEnScreenSaver) ? (mExtra % 7) : 0);

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
