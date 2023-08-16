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
            mDispWidth = 0;
        }

        void init(uint8_t type, uint8_t rotation, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t *utcTs, const char *version) {

            u8g2_cb_t *rot = (u8g2_cb_t *)((rotation != 0x00) ? U8G2_R2 : U8G2_R0);
            mType = type;
            mDisplay = new U8G2_PCD8544_84X48_F_4W_SW_SPI(rot, clock, data, cs, dc, reset);

            mUtcTs = utcTs;

            mDisplay->begin();
            mDispWidth = mDisplay->getDisplayWidth();
            calcLinePositions();

            mDisplay->clearBuffer();
            mDisplay->setContrast(mLuminance);

            printText("AHOY!", l_Ahoy);
            printText("ahoydtu.de", l_Website);
            printText(version, l_Version);
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
            mDisplay->setContrast(mLuminance);

            if ((totalPower > 0) && (isprod > 0)) {
                mTimeout = DISP_DEFAULT_TIMEOUT;
                mDisplay->setPowerSave(false);

                if (totalPower > 999)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kW", (totalPower / 1000));
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f W", totalPower);

                printText(mFmtText, l_TotalPower);
            } else {
                printText("offline", l_TotalPower);
                // check if it's time to enter power saving mode
                if (mTimeout == 0)
                    mDisplay->setPowerSave(mEnPowerSafe);
            }

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "Today: %4.0f Wh", totalYieldDay);
            printText(mFmtText, l_YieldDay);

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "Total: %.1f kWh", totalYieldTotal);
            printText(mFmtText, l_YieldTotal);

            if (NULL != mUtcTs)
                printText(ah::getDateTimeStrShort(gTimezone.toLocal(*mUtcTs)).c_str(), l_Time);

            IPAddress ip = WiFi.localIP();
            if (!(mExtra % 5) && (ip))
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%s", ip.toString().c_str());
            else
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "Inv.On: %d", isprod);
            printText(mFmtText, l_Status);

            mDisplay->sendBuffer();

            mExtra++;
        }

    private:
        uint16_t mDispWidth;
        enum _dispLine {
            // start page
            l_Website = 0,
            l_Ahoy = 2,
            l_Version = 4,
            // run page
            l_Time = 0,
            l_Status = 1,
            l_TotalPower = 2,
            l_YieldDay = 3,
            l_YieldTotal = 4,
            // End
            l_MAX_LINES = 5,
        };

        void calcLinePositions() {
            uint8_t yOff = 0;
            uint8_t i = 0;
            uint8_t asc, dsc;

            do {
                setFont(i);
                asc = mDisplay->getAscent();
                yOff += asc;
                mLineYOffsets[i] = yOff;
                dsc = mDisplay->getDescent();
                if (l_TotalPower!=i)   // power line needs no descent spacing
                    yOff -= dsc;
                yOff++;     // instead lets spend one pixel space between all lines
                i++;
            } while(l_MAX_LINES>i);
        }

        inline void setFont(uint8_t line) {
            if ((line == l_TotalPower) || (line == l_Ahoy))
                mDisplay->setFont(u8g2_font_logisoso16_tr);
            else
                mDisplay->setFont(u8g2_font_5x8_tr);
        }

        void printText(const char *text, uint8_t line) {
            uint8_t dispX;
            setFont(line);
            dispX = (mDispWidth - mDisplay->getStrWidth(text)) / 2;  // center text
            dispX += (mEnScreenSaver) ? (mExtra % 7) : 0;
            mDisplay->drawStr(dispX, mLineYOffsets[line], text);
        }
};
