//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "Display_Mono.h"

class DisplayMono128X64 : public DisplayMono {
    public:
        DisplayMono128X64() : DisplayMono() {
            mEnPowerSave = true;
            mEnScreenSaver = true;
            mLuminance = 60;
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
            uint8_t pos, sun_pos, moon_pos;

            // Test
            /*
            mDisplayData->isSleeping = 1;
            mDisplayData->isProducing = 0;
            mDisplayData->totalPower = 10000;
            mDisplayData->totalYieldDay = 4998;
            mDisplayData->totalYieldTotal = 5321;
            */

            mDisplay->clearBuffer();

            
            // set Contrast of the Display to raise the lifetime
            mDisplay->setContrast(mLuminance);

            // print total power
            if (mDisplayData->nrProducing > 0) {
                mTimeout = DISP_DEFAULT_TIMEOUT;
                mDisplay->setPowerSave(false);

                if (mDisplayData->totalPower > 999)
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.2f kW", (mDisplayData->totalPower / 1000));
                else
                    snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%.0f W", mDisplayData->totalPower);
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
                    sun_pos = mDisplay->getStrWidth(mFmtText);
                    snprintf(mFmtText+2, DISP_FMT_TEXT_LEN, "   %2d", mDisplayData->nrSleeping);
                    moon_pos = mDisplay->getStrWidth(mFmtText);
                    snprintf(mFmtText+7, DISP_FMT_TEXT_LEN, " ");
                }
                printText(mFmtText, l_Status, 0xff);

                pos = (mDispWidth - mDisplay->getStrWidth(mFmtText)) / 2;
                mDisplay->setFont(u8g2_font_ncenB08_symbols8_ahoy);
                if (sun_pos!=-1)
                    mDisplay->drawStr(pos+sun_pos, mLineYOffsets[l_Status], "G");        // sun
                if (moon_pos!=-1)
                    mDisplay->drawStr(pos+moon_pos, mLineYOffsets[l_Status], "H");    // moon
            }

            // print yields
            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%7.0f Wh", mDisplayData->totalYieldDay);
            printText(mFmtText, l_YieldDay, 25);
            snprintf(mFmtText, DISP_FMT_TEXT_LEN, "%7.1f kWh", mDisplayData->totalYieldTotal);
            printText(mFmtText, l_YieldTotal, 25);
            mDisplay->setFont(u8g2_font_ncenB10_symbols10_ahoy);
            mDisplay->drawStr(11, mLineYOffsets[l_YieldDay],   "I");    // day
            mDisplay->drawStr(11, mLineYOffsets[l_YieldTotal], "D");    // total

            // draw dynamic RSSI bars
            int rssi_bar_height = 9;
            for (int i=0; i<4;i++) {
                int radio_rssi_threshold = -60 - i*10;
                int wifi_rssi_threshold = -60 - i*10;
                if (mDisplayData->RadioRSSI > radio_rssi_threshold)
                    mDisplay->drawBox(0,              8+(rssi_bar_height+1)*i,  4-i,rssi_bar_height);
                if (mDisplayData->WifiRSSI > wifi_rssi_threshold)
                    mDisplay->drawBox(mDispWidth-4+i, 8+(rssi_bar_height+1)*i,  4-i,rssi_bar_height);
            }

            // draw dynamic antenna and WiFi symbols
            mDisplay->setFont(u8g2_font_ncenB10_symbols10_ahoy);
            char sym[]=" ";
            sym[0] = mDisplayData->RadioSymbol?'A':'E';                 // NRF
            mDisplay->drawStr(0, mLineYOffsets[l_RSSI], sym);

            if (mDisplayData->MQTTSymbol)
                sym[0] = 'J'; // MQTT
            else
                sym[0] = mDisplayData->WifiSymbol?'B':'F';                  // Wifi
            mDisplay->drawStr(mDispWidth - mDisplay->getStrWidth(sym), mLineYOffsets[l_RSSI], sym);
            mDisplay->sendBuffer();

            mExtra++;

            /* // just for test
            mDisplay->drawPixel(0, 0);
            mDisplay->drawPixel(mDispWidth-1, 0);
            mDisplay->drawPixel(0, mDispHeight-1);
            mDisplay->drawPixel(mDispWidth-1, mDispHeight-1);
            */

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
            mDisplay->drawStr(dispX, mLineYOffsets[line], text);
        }
};
