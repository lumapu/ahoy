//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"

class DisplayMono84X48 : public DisplayMono {
    public:
        DisplayMono84X48() : DisplayMono() {
            mExtra = 0;
        }

        void config(display_t *cfg) override {
            mCfg = cfg;
        }

        void init(DisplayData *displayData) override {
            u8g2_cb_t *rot = (u8g2_cb_t *)((mCfg->rot != 0x00) ? U8G2_R2 : U8G2_R0);

            monoInit(new U8G2_PCD8544_84X48_F_4W_SW_SPI(rot, mCfg->disp_clk, mCfg->disp_data, mCfg->disp_cs, mCfg->disp_dc, 0xff), displayData);
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

            if (mCfg->graph_ratio > 0)
                initPowerGraph(mDispWidth - 16, mLineYOffsets[graph_last_line] - mLineYOffsets[graph_first_line - 1] - 2);

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
            mDisplayData->totalPower = 19897.6; // W
            mDisplayData->totalYieldDay = 15521.9; // Wh
            mDisplayData->totalYieldTotal = 654321.9; // kWh
            mDisplay->drawPixel(0, 0);
            mDisplay->drawPixel(mDispWidth-1, 0);
            mDisplay->drawPixel(0, mDispHeight-1);
            mDisplay->drawPixel(mDispWidth-1, mDispHeight-1);
            */

            // add new power data to power graph
            if (mDisplayData->nrProducing > 0) {
                addPowerGraphEntry(mDisplayData->totalPower);
            }

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
                    if (0 == mDisplayData->nrSleeping + mDisplayData->nrProducing)
                        snprintf(mFmtText, DISP_FMT_TEXT_LEN, STR_NO_INVERTER);
                    else if (0 == mDisplayData->nrSleeping)
                        snprintf(mFmtText, DISP_FMT_TEXT_LEN, "\x86");      // sun symbol
                    else if (0 == mDisplayData->nrProducing)
                        snprintf(mFmtText, DISP_FMT_TEXT_LEN, "\x87");      // moon symbol
                    else
                        snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%d\x86 %d\x87", mDisplayData->nrProducing, mDisplayData->nrSleeping);
                    printText(mFmtText, l_Status, 0xff);
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
                printText("\x88", l_YieldDay,   10);        // day symbol
                if (mDisplayData->totalYieldDay > 9999.0)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kWh", mDisplayData->totalYieldDay / 1000.0);
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f Wh", mDisplayData->totalYieldDay);
                printText(mFmtText, l_YieldDay, 0xff);
            }

            if (showLine(l_YieldTotal)) {
                // print total yield
                printText("\x83", l_YieldTotal, 10);        // total symbol
                if (mDisplayData->totalYieldTotal > 9999.0)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f MWh", mDisplayData->totalYieldTotal / 1000.0);
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f kWh", mDisplayData->totalYieldTotal);
                printText(mFmtText, l_YieldTotal, 0xff);
            }

            if ((mCfg->graph_ratio > 0) && (mDispSwitchState == DispSwitchState::GRAPH)) {
                // plot power graph
                plotPowerGraph(8, mLineYOffsets[graph_last_line] - 1);
            }

            // draw dynamic RSSI bars
            int rssi_bar_height = 7;
            for (int i = 0; i < 4; i++) {
                int rssi_threshold = -60 - i * 10;
                uint8_t barwidth = std::min(4 - i, 3);
                if (mDisplayData->RadioRSSI > rssi_threshold)
                    mDisplay->drawBox(0,                     8 + (rssi_bar_height + 1) * i, barwidth, rssi_bar_height);
                if (mDisplayData->WifiRSSI > rssi_threshold)
                    mDisplay->drawBox(mDispWidth - barwidth, 8 + (rssi_bar_height + 1) * i, barwidth, rssi_bar_height);
            }

            // draw dynamic antenna and WiFi symbols
            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%c", mDisplayData->RadioSymbol?'\x80':'\x84'); // NRF
            printText(mFmtText, l_RSSI);
            if (mDisplayData->MQTTSymbol)
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "\x89"); // MQTT
            else
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%c", mDisplayData->WifiSymbol?'\x81':'\x85'); // Wifi connected
            printText(mFmtText, l_RSSI, mDispWidth - 6);

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

        void calcLinePositions() {
            uint8_t yOff = 0;
            uint8_t i = 0;

            do {
                setLineFont(i);
                uint8_t asc = mDisplay->getAscent();
                yOff += asc;
                mLineYOffsets[i] = yOff;
                uint8_t dsc = mDisplay->getDescent();
                if (l_TotalPower != i)   // power line needs no descent spacing
                    yOff -= dsc;
                yOff++;     // instead lets spend one pixel space between all lines
                i++;
            } while(l_MAX_LINES > i);
        }

        inline void setLineFont(uint8_t line) {
            if (line == l_TotalPower) // || (line == l_Ahoy) -> l_TotalPower == l_Ahoy == 2
                mDisplay->setFont(u8g2_font_logisoso16_tr);
            else
                mDisplay->setFont(u8g2_font_5x8_symbols_ahoy);
        }

        void printText(const char *text, uint8_t line, uint8_t col=0) {
            uint8_t dispX;

            setLineFont(line);
            if (0xff == col)
                dispX = (mDispWidth - mDisplay->getStrWidth(text)) / 2;  // center text
            else
                dispX = col;
            mDisplay->drawStr(dispX, mLineYOffsets[line], text);
        }

        bool showLine(uint8_t line) {
            return ((mDispSwitchState == DispSwitchState::TEXT) || ((line < graph_first_line) || (line > graph_last_line)));
        }
};


