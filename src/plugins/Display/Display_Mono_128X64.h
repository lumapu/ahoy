//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display.h"
#include "Display_Mono.h"

class DisplayMono128X64 : public DisplayMono {
    public:
        DisplayMono128X64() : DisplayMono() {
            mExtra = 0;
        }

        void config(display_t *cfg) override {
            mCfg = cfg;
        }

        void init(DisplayData *displayData) override {
            u8g2_cb_t *rot = (u8g2_cb_t *)(( mCfg->rot != 0x00) ? U8G2_R2 : U8G2_R0);
            switch (mCfg->type) {
                case DISP_TYPE_T1_SSD1306_128X64:
                    monoInit(new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(rot, 0xff, mCfg->disp_clk, mCfg->disp_data), displayData);
                    break;
                case DISP_TYPE_T2_SH1106_128X64:
                    monoInit(new U8G2_SH1106_128X64_NONAME_F_HW_I2C(rot, 0xff, mCfg->disp_clk, mCfg->disp_data), displayData);
                    break;
                case DISP_TYPE_T6_SSD1309_128X64:
                default:
                    monoInit(new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(rot, 0xff, mCfg->disp_clk, mCfg->disp_data), displayData);
                    break;
            }
            calcLinePositions();

            switch(mCfg->graph_size) { // var opts2 = [[0, "Line 1 - 2"], [1, "Line 2 - 3"], [2, "Line 1 - 3"], [3, "Line 2 - 4"], [4, "Line 1 - 4"]];
                case 0:
                    graph_first_line = 1;
                    graph_last_line = 2;
                    break;
                case 1:
                    graph_first_line = 2;
                    graph_last_line = 3;
                    break;
                case 2:
                    graph_first_line = 1;
                    graph_last_line = 3;
                    break;
                case 3:
                    graph_first_line = 2;
                    graph_last_line = 4;
                    break;
                case 4:
                default:
                    graph_first_line = 1;
                    graph_last_line = 4;
                    break;
            }

            widthShrink = (mCfg->screenSaver == 1) ? pixelShiftRange : 0;  // shrink graphwidth for pixelshift screensaver

            if (mCfg->graph_ratio > 0)
                initPowerGraph(mDispWidth - 22 - widthShrink, mLineYOffsets[graph_last_line] - mLineYOffsets[graph_first_line - 1] - 2);

            printText("Ahoy!", l_Ahoy, 0xff);
            printText("ahoydtu.de", l_Website, 0xff);
            printText(mDisplayData->version, l_Version, 0xff);
            mDisplay->sendBuffer();
        }

        void disp(void) override {
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

            // add new power data to power graph
            if (mDisplayData->nrProducing > 0)
                addPowerGraphEntry(mDisplayData->totalPower);

            // print Date and time
            if (0 != mDisplayData->utcTs)
                printText(ah::getDateTimeStrShort_i18n(mDisplayData->utcTs).c_str(), l_Time, 0xff);

            if (showLine(l_Status)) {
                // alternatively:
                // print ip address
                if (!(mExtra % 5) && (mDisplayData->ipAddress)) {
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%s", (mDisplayData->ipAddress).toString().c_str());
                    printText(mFmtText, l_Status, 0xff);
                }
                // print status of inverters
                else {
                    int8_t sun_pos = -1;
                    int8_t moon_pos = -1;
                    setLineFont(l_Status);
                    if (0 == mDisplayData->nrSleeping + mDisplayData->nrProducing)
                        snprintf(mFmtText, DISP_FMT_TEXT_LEN, STR_NO_INVERTER);
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

                    uint8_t pos = (mDispWidth - mDisplay->getStrWidth(mFmtText)) / 2;
                    mDisplay->setFont(u8g2_font_ncenB08_symbols8_ahoy);
                    if (sun_pos != -1)
                        mDisplay->drawStr(pos + sun_pos + mPixelshift, mLineYOffsets[l_Status], "G");     // sun symbol
                    if (moon_pos != -1)
                        mDisplay->drawStr(pos + moon_pos + mPixelshift, mLineYOffsets[l_Status], "H");    // moon symbol
                }
            }

            if (showLine(l_TotalPower)) {
                // print total power
                if (mDisplayData->nrProducing > 0) {
                    if (mDisplayData->totalPower > 9999.0)
                        snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kW", (mDisplayData->totalPower / 1000.0));
                    else
                        snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f W", mDisplayData->totalPower);

                    printText(mFmtText, l_TotalPower, 0xff);
                } else {
                    printText(STR_OFFLINE, l_TotalPower, 0xff);
                }
            }

            if (showLine(l_YieldDay)) {
                // print day yield
                mDisplay->setFont(u8g2_font_ncenB10_symbols10_ahoy);
                mDisplay->drawStr(16 + mPixelshift, mLineYOffsets[l_YieldDay],   "I");    // day symbol
                mDisplay->drawStr(16 + mPixelshift, mLineYOffsets[l_YieldTotal], "D");    // total symbol

                if (mDisplayData->totalYieldDay > 9999.0)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kWh", mDisplayData->totalYieldDay / 1000.0);
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f Wh", mDisplayData->totalYieldDay);
                printText(mFmtText, l_YieldDay, 0xff);
            }

            if (showLine(l_YieldTotal)) {
                // print total yield
                if (mDisplayData->totalYieldTotal > 9999.0)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f MWh", mDisplayData->totalYieldTotal / 1000.0);
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f kWh", mDisplayData->totalYieldTotal);
                printText(mFmtText, l_YieldTotal, 0xff);
            }

            if ((mCfg->graph_ratio > 0) && (mDispSwitchState == DispSwitchState::GRAPH)) {
                // plot power graph
                plotPowerGraph((mDispWidth - mPgWidth) / 2 + mPixelshift, mLineYOffsets[graph_last_line] - 1);
            }

            // draw dynamic RSSI bars
            int rssi_bar_height = 9;
            for (int i = 0; i < 4; i++) {
                int rssi_threshold = -60 - i * 10;
                uint8_t barwidth = std::min(4 - i, 3);
                if (mDisplayData->RadioRSSI > rssi_threshold)
                    mDisplay->drawBox(widthShrink / 2 + mPixelshift,                         8 + (rssi_bar_height + 1) * i, barwidth, rssi_bar_height);
                if (mDisplayData->WifiRSSI > rssi_threshold)
                    mDisplay->drawBox(mDispWidth - barwidth - widthShrink / 2 + mPixelshift, 8 + (rssi_bar_height + 1) * i, barwidth, rssi_bar_height);
            }
            // draw dynamic antenna and WiFi symbols
            mDisplay->setFont(u8g2_font_ncenB10_symbols10_ahoy);
            char sym[]=" ";
            sym[0] = mDisplayData->RadioSymbol?'A':'E';                 // NRF
            mDisplay->drawStr((widthShrink / 2) + mPixelshift, mLineYOffsets[l_RSSI], sym);

            if (mDisplayData->MQTTSymbol)
                sym[0] = 'J'; // MQTT
            else
                sym[0] = mDisplayData->WifiSymbol?'B':'F';              // Wifi
            mDisplay->drawStr(mDispWidth - mDisplay->getStrWidth(sym) - (widthShrink / 2) + mPixelshift, mLineYOffsets[l_RSSI], sym);
            mDisplay->sendBuffer();

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

        uint8_t graph_first_line = 0;
        uint8_t graph_last_line = 0;

        const uint8_t pixelShiftRange = 11;  // number of pixels to shift from left to right (centered -> must be odd!)
        uint8_t widthShrink = 0;

        void calcLinePositions() {
            uint8_t yOff = 0;
            uint8_t i = 0;

            do {
                setLineFont(i);
                uint8_t asc = mDisplay->getAscent();
                yOff += asc;
                mLineYOffsets[i] = yOff;
                uint8_t dsc = mDisplay->getDescent();
                yOff -= dsc;
                if (l_Time == i) // prevent time and status line to touch
                    yOff++;      // -> one pixels space
                i++;
            } while(l_MAX_LINES>i);
        }

        inline void setLineFont(uint8_t line) {
            if (line == l_TotalPower) // || (line == l_Ahoy) -> l_TotalPower == l_Ahoy == 2
                mDisplay->setFont(u8g2_font_ncenB14_tr);
            else if ((line == l_YieldDay) ||
                     (line == l_YieldTotal))
                // mDisplay->setFont(u8g2_font_5x8_symbols_ahoy);
                mDisplay->setFont(u8g2_font_ncenB10_tr);
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

        bool showLine(uint8_t line) {
            return ((mDispSwitchState == DispSwitchState::TEXT) || ((line < graph_first_line) || (line > graph_last_line)));
        }
};
