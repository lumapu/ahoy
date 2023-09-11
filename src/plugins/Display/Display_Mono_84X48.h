//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"
#include "../../utils/dbg.h"

class DisplayMono84X48 : public DisplayMono {
    public:
        DisplayMono84X48() : DisplayMono() {
            mEnPowerSave = true;
            mEnScreenSaver = true;
            mLuminance = 140;
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
            monoInit(new U8G2_PCD8544_84X48_F_4W_SW_SPI(rot, clock, data, cs, dc, reset), type, displayData);
            calcLinePositions();
            printText("Ahoy!", l_Ahoy, 0xff);
            printText("ahoydtu.de", l_Website, 0xff);
            printText(mDisplayData->version, l_Version, 0xff);
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
            // Test
            /*
            mDisplayData->nrSleeping = 10;
            mDisplayData->nrProducing = 10;
            mDisplayData->totalPower = 12345.67;
            mDisplayData->totalYieldDay = 12345.67;
            mDisplayData->totalYieldTotal = 1234;
            mDisplayData->utcTs += 1000000;
            */

            mDisplay->clearBuffer();

            // print total power
            if (mDisplayData->nrProducing > 0) {
                mTimeout = DISP_DEFAULT_TIMEOUT;
                mDisplay->setPowerSave(false);

                if (mDisplayData->totalPower > 9999)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2fkW", (mDisplayData->totalPower / 1000)); // forgo spacing between value and SI unit in favor of second position after decimal point
                else if (mDisplayData->totalPower > 999)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kW", (mDisplayData->totalPower / 1000));
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.1f W", mDisplayData->totalPower);

                printText(mFmtText, l_TotalPower, 0xff);
            } else {
                printText("offline", l_TotalPower, 0xff);
                // check if it's time to enter power saving mode
                if (mTimeout == 0)
                    mDisplay->setPowerSave(mEnPowerSave);
            }

            // print Date and time
            if (0 != mDisplayData->utcTs)
                printText(ah::getDateTimeStrShort(gTimezone.toLocal(mDisplayData->utcTs)).c_str(), l_Time, 0xff);

            // alternatively:
            // print ip address
            if (!(mExtra % 5) && (mDisplayData->ipAddress)) {
                snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%s", (mDisplayData->ipAddress).toString().c_str());
                printText(mFmtText, l_Status, 0xff);
            }
            // print status of inverters
            else {
                if (0 == mDisplayData->nrSleeping)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "\x86");
                else if (0 == mDisplayData->nrProducing)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "\x87");
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%d\x86 %d\x87", mDisplayData->nrProducing, mDisplayData->nrSleeping);
                setLineFont(l_Status);
                printText(mFmtText, l_Status, (mDispWidth - mDisplay->getStrWidth(mFmtText)) / 2);
            }

            // print yields
            printText("\x88", l_YieldDay,   11); // day symbol
            printText("\x83", l_YieldTotal, 11); // total symbol
            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%7.0f Wh", mDisplayData->totalYieldDay);
            printText(mFmtText, l_YieldDay, 18);
            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%7.1f kWh", mDisplayData->totalYieldTotal);
            printText(mFmtText, l_YieldTotal, 18);

            // draw dynamic Nokia RSSI bars
            int rssi_bar_height = 7;
            for (int i=0; i<4;i++) {
                int radio_rssi_threshold = -60 - i*10; // radio rssi not yet tested in reality!
                int wifi_rssi_threshold = -60 - i*10;
                if (mDisplayData->RadioRSSI > radio_rssi_threshold)
                    mDisplay->drawBox(0,              8+(rssi_bar_height+1)*i,  4-i,rssi_bar_height);
                if (mDisplayData->WifiRSSI > wifi_rssi_threshold)
                    mDisplay->drawBox(mDispWidth-4+i, 8+(rssi_bar_height+1)*i,  4-i,rssi_bar_height);
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
                if (l_TotalPower!=i)   // power line needs no descent spacing
                    yOff -= dsc;
                yOff++;     // instead lets spend one pixel space between all lines
                i++;
            } while(l_MAX_LINES>i);
        }

        inline void setLineFont(uint8_t line) {
            if ((line == l_TotalPower) || (line == l_Ahoy))
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
};


