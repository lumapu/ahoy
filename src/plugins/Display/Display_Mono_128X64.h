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
            mExtra = 0;
        }

        void config(bool enPowerSave, uint8_t screenSaver, uint8_t lum) {
            mEnPowerSave = enPowerSave;
            mScreenSaver = screenSaver;
            mLuminance = lum;
        }

        void init(uint8_t type, uint8_t rotation, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, DisplayData *displayData) {
            u8g2_cb_t *rot = (u8g2_cb_t *)((rotation != 0x00) ? U8G2_R2 : U8G2_R0);
            switch (type) {
                case 1:
                    monoInit(new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(rot, reset, clock, data), type, displayData);
                    break;
                case 2:
                    monoInit(new U8G2_SH1106_128X64_NONAME_F_HW_I2C(rot, reset, clock, data), type, displayData);
                    break;
                case 6:
                default:
                    monoInit(new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(rot, reset, clock, data), type, displayData);
                    break;
            }
            calcLinePositions();

            printText("Ahoy!", l_Ahoy, 0xff);
            printText("ahoydtu.de", l_Website, 0xff);
            printText(mDisplayData->version, l_Version, 0xff);
            mDisplay->sendBuffer();
        }

        void disp(void) {
            uint8_t pos, sun_pos, moon_pos;
#ifdef DISPLAY_CHART
            bool drawChart = true;  // should come from settings
            static unsigned int dbgCnt = 0;

            // We are called every 10 seconds (see Display::tickerSecond() )
            if ((((dbgCnt++) % 6) != 0))
                return;  // update only every minute.
#endif
            mDisplay->clearBuffer();

            // Layout-Test
            /*
            mDisplayData->nrSleeping = 10;
            mDisplayData->nrProducing = 10;
            mDisplayData->totalPower = 15432.9; // W
            mDisplayData->totalYieldDay = 14321.9; // Wh
            mDisplayData->totalYieldTotal = 15432.9; // kWh
            mDisplay->drawPixel(0, 0);
            mDisplay->drawPixel(mDispWidth-1, 0);
            mDisplay->drawPixel(0, mDispHeight-1);
            mDisplay->drawPixel(mDispWidth-1, mDispHeight-1);
            */

            // calculate current pixelshift for pixelshift screensaver
            calcPixelShift(pixelShiftRange);

            // print total power
            if (mDisplayData->nrProducing > 0) {
                if (mDisplayData->totalPower > 9999.0)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kW", (mDisplayData->totalPower / 1000.0));
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f W", mDisplayData->totalPower);

                printText(mFmtText, l_TotalPower, 0xff);
            } else {
                printText("offline", l_TotalPower, 0xff);
            }



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

            // print Date and time
            if (0 != mDisplayData->utcTs)
                printText(ah::getDateTimeStrShort(gTimezone.toLocal(mDisplayData->utcTs)).c_str(), l_Time, 0xff);
            
            // dynamic status bar, alternatively:
            // print ip address
            if (!(mExtra % 5) && (mDisplayData->ipAddress)) {
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%s", (mDisplayData->ipAddress).toString().c_str());
                printText(mFmtText, l_Status, 0xff);
            }
            // print status of inverters
            else {
                sun_pos = -1;
                moon_pos = -1;
                setLineFont(l_Status);
                if (0 == mDisplayData->nrSleeping + mDisplayData->nrProducing)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "no inverter");
                else if (0 == mDisplayData->nrSleeping) {
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "  ");
                    sun_pos = 0;
                }
                else if (0 == mDisplayData->nrProducing) {
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "  ");
                    moon_pos = 0;
                }
                else {
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%2d", mDisplayData->nrProducing);
                    sun_pos = mDisplay->getStrWidth(mFmtText) + 1;
                    snprintf(mFmtText+2, DISP_FMT_TEXT_LEN, "   %2d", mDisplayData->nrSleeping);
                    moon_pos = mDisplay->getStrWidth(mFmtText) + 1;
                    snprintf(mFmtText+7, DISP_FMT_TEXT_LEN, " ");
                }
                printText(mFmtText, l_Status, 0xff);

                pos = (mDispWidth - mDisplay->getStrWidth(mFmtText)) / 2;
                mDisplay->setFont(u8g2_font_ncenB08_symbols8_ahoy);
                if (sun_pos!=-1)
                    mDisplay->drawStr(pos + sun_pos + mPixelshift, mLineYOffsets[l_Status], "G");     // sun symbol
                if (moon_pos!=-1)
                    mDisplay->drawStr(pos + moon_pos + mPixelshift, mLineYOffsets[l_Status], "H");    // moon symbol
            }

            // print yields
            mDisplay->setFont(u8g2_font_ncenB10_symbols10_ahoy);
            mDisplay->drawStr(16 + mPixelshift, mLineYOffsets[l_YieldDay],   "I");    // day symbol
            mDisplay->drawStr(16 + mPixelshift, mLineYOffsets[l_YieldTotal], "D");    // total symbol

            if (mDisplayData->totalYieldDay > 9999.0)
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kWh", mDisplayData->totalYieldDay / 1000.0);
            else
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f Wh", mDisplayData->totalYieldDay);
            printText(mFmtText, l_YieldDay, 0xff);

            if (mDisplayData->totalYieldTotal > 9999.0)
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f MWh", mDisplayData->totalYieldTotal / 1000.0);
            else
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f kWh", mDisplayData->totalYieldTotal);
            printText(mFmtText, l_YieldTotal, 0xff);

            // draw dynamic RSSI bars
            int xoffs;
            if (mScreenSaver == 1) // shrink screenwidth for pixelshift screensaver
                xoffs = pixelShiftRange/2;
            else
                xoffs = 0;
            int rssi_bar_height = 9;
            for (int i = 0; i < 4; i++) {
                int radio_rssi_threshold = -60 - i * 10;
                int wifi_rssi_threshold = -60 - i * 10;
                if (mDisplayData->RadioRSSI > radio_rssi_threshold)
                    mDisplay->drawBox(xoffs + mPixelshift, 8 + (rssi_bar_height + 1) * i, 4 - i, rssi_bar_height);
                if (mDisplayData->WifiRSSI > wifi_rssi_threshold)
                    mDisplay->drawBox(mDispWidth - 4 - xoffs + mPixelshift + i, 8 + (rssi_bar_height + 1) * i, 4 - i, rssi_bar_height);
            }
            // draw dynamic antenna and WiFi symbols
            mDisplay->setFont(u8g2_font_ncenB10_symbols10_ahoy);
            char sym[]=" ";
            sym[0] = mDisplayData->RadioSymbol?'A':'E';                 // NRF
            mDisplay->drawStr(xoffs + mPixelshift, mLineYOffsets[l_RSSI], sym);

            if (mDisplayData->MQTTSymbol)
                sym[0] = 'J'; // MQTT
            else
                sym[0] = mDisplayData->WifiSymbol?'B':'F';              // Wifi
            mDisplay->drawStr(mDispWidth - mDisplay->getStrWidth(sym) - xoffs + mPixelshift, mLineYOffsets[l_RSSI], sym);
            mDisplay->sendBuffer();


			     }
			     mDisplay->sendBuffer();

			     mDispY = 0;
           mExtra++;
        }

    private:
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
            // run page - rssi bar symbols
            l_RSSI = 4,
            // End
            l_MAX_LINES = 5,
        };

        const uint8_t pixelShiftRange = 11;  // number of pixels to shift from left to right (centered -> must be odd!)

        void calcLinePositions() {
            uint8_t yOff = 0;
            uint8_t i = 0;
            uint8_t asc, dsc;

            do {
                setLineFont(i);
                asc = mDisplay->getAscent();
                yOff += asc;
                mLineYOffsets[i] = yOff;
                dsc = mDisplay->getDescent();
                yOff -= dsc;
                if (l_Time==i)   // prevent time and status line to touch
                    yOff+=1;     // -> one pixels space
                i++;
            } while(l_MAX_LINES>i);
        }


        inline void setLineFont(uint8_t line) {
            if ((line == l_TotalPower) ||
                (line == l_Ahoy))
#ifdef DISPLAY_CHART
					      mDisplay->setFont(u8g2_font_6x12_tr);
#else
					      mDisplay->setFont(u8g2_font_ncenB14_tr);
#endif
            else if ((line == l_YieldDay) ||
                     (line == l_YieldTotal))
                // mDisplay->setFont(u8g2_font_5x8_symbols_ahoy);
#ifdef DISPLAY_CHART
					      mDisplay->setFont(u8g2_font_5x8_tr);
#else
					      mDisplay->setFont(u8g2_font_ncenB10_tr);
#endif
            else
                mDisplay->setFont(u8g2_font_ncenB08_tr);
        }

        void printText(const char *text, uint8_t line, uint8_t col=0) {
            uint8_t dispX;
            setLineFont(line);
            if (0xff == col)
                dispX = (mDispWidth - mDisplay->getStrWidth(text)) / 2;  // center text
            else
                dispX = col;
            dispX += mPixelshift;
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
