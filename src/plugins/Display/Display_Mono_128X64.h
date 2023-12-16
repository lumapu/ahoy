//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"

class DisplayMono128X64 : public DisplayMono {
    public:
        DisplayMono128X64() : DisplayMono() {
            mEnPowerSafe = true;
            mEnScreenSaver = true;
            mLuminance = 60;
            mDispY = 0;
            mTimeout = DISP_DEFAULT_TIMEOUT;  // interval at which to power save (milliseconds)
            mUtcTs = NULL;
            mType = 0;
#ifdef DISPLAY_CHART
			for (int i = 0; i < DISP_WATT_ARR_LENGTH; i++)
				m_wattArr[i] = 0.0;
			m_wattListIdx = 0;
			m_wattDispIdx = 1;
#endif
		}

        void init(uint8_t type, uint8_t rotation, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t *utcTs, const char *version) {

            u8g2_cb_t *rot = (u8g2_cb_t *)((rotation != 0x00) ? U8G2_R2 : U8G2_R0);
            mType = type;

            switch (type) {
                case 1:
                    mDisplay = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(rot, reset, clock, data);
                    break;
                default:
                case 2:
                    mDisplay = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(rot, reset, clock, data);
                    break;
            }

            mUtcTs = utcTs;

            mDisplay->begin();
            calcLinePositions();

            mDisplay->clearBuffer();
            mDisplay->setContrast(mLuminance);
            printText("AHOY!", 0, 35);
            printText("ahoydtu.de", 2, 20);
            printText(version, 3, 46);
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
#ifdef DISPLAY_CHART
            bool drawChart = true;  // should come from settings
            static unsigned int dbgCnt = 0;

            // We are called every 10 seconds (see Display::tickerSecond() )
            if ((((dbgCnt++) % 6) != 0))
                return;  // update only every minute.
#endif
            mDisplay->clearBuffer();

            // set Contrast of the Display to raise the lifetime
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
                printText("offline", 0, 25);
                // check if it's time to enter power saving mode
                if (mTimeout == 0)
                mDisplay->setPowerSave(mEnPowerSafe);
            }

            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "today: %4.0f Wh", totalYieldDay);
            printText(mFmtText, 1);

#ifdef DISPLAY_CHART
			if (drawChart)
			{
				m_wattArr[m_wattListIdx] = totalPower;
				drawPowerChart();
				m_wattListIdx = (m_wattListIdx + 1) % (DISP_WATT_ARR_LENGTH);
				m_wattDispIdx = (m_wattDispIdx + 1) % (DISP_WATT_ARR_LENGTH);
			}
			else
			{
#else
    		{
            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "total: %.1f kWh", totalYieldTotal);
            printText(mFmtText, 2);

            IPAddress ip = WiFi.localIP();
            if (!(mExtra % 10) && (ip))
                printText(ip.toString().c_str(), 3);
            else if (!(mExtra % 5)) {
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%d Inverter on", isprod);
                printText(mFmtText, 3);
            } else  if (NULL != mUtcTs)
                printText(ah::getDateTimeStr(gTimezone.toLocal(*mUtcTs)).c_str(), 3);
#endif
			}
			mDisplay->sendBuffer();

			mDispY = 0;
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
#ifdef DISPLAY_CHART
					mDisplay->setFont(u8g2_font_6x12_tr);
#else
					mDisplay->setFont(u8g2_font_ncenB14_tr);
#endif
                    break;
                case 3:
                    mDisplay->setFont(u8g2_font_5x8_tr);
                    break;
                default:
#ifdef DISPLAY_CHART
					mDisplay->setFont(u8g2_font_5x8_tr);
#else
					mDisplay->setFont(u8g2_font_ncenB10_tr);
#endif
					break;
            }
        }
        void printText(const char *text, uint8_t line, uint8_t dispX = 5) {
            setFont(line);

            dispX += (mEnScreenSaver) ? (mExtra % 7) : 0;
            mDisplay->drawStr(dispX, mLineYOffsets[line], text);
        }

#ifdef DISPLAY_CHART

		void drawPowerChart()
		{
			const int hight = 32; // chart hight

			// Clear area
			// mDisplay->draw_rectangle(0, 63 - hight, DISP_WATT_ARR_LENGTH, 63, OLED::SOLID, OLED::BLACK);
			mDisplay->setDrawColor(0);
			mDisplay->drawBox(0, 63 - hight, DISP_WATT_ARR_LENGTH, hight);
			mDisplay->setDrawColor(1);

			// Get max value for scaling
			float maxValue = 0.0;
			for (int i = 0; i < DISP_WATT_ARR_LENGTH; i++)
			{
				float fValue = m_wattArr[i];
				if (fValue > maxValue)
					maxValue = fValue;
			}
			// calc divider to fit into chart hight
			int divider = round(maxValue / (float)hight);
			if (divider < 1)
				divider = 1;

			// draw chart bars
			int idx = m_wattDispIdx;
			for (int i = 0; i < DISP_WATT_ARR_LENGTH; i++)
			{
				float fValue = m_wattArr[idx];
				idx = (idx + 1) % (DISP_WATT_ARR_LENGTH);
				int iValue = roundf(fValue);
				iValue /= divider;
				if (iValue > hight)
					iValue = hight;
				// mDisplay->draw_line(i, 63 - iValue, i, 63);
				// mDisplay->drawVLine(i, 63 - iValue, iValue);
				mDisplay->drawLine(i, 63 - iValue, i, 63);
			}
		}
#endif
};
