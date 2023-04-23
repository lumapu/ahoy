// SPDX-License-Identifier: GPL-2.0-or-later
#include "Display_Mono.h"

#ifdef ESP8266
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif
#include "../../utils/helper.h"

//#ifdef U8X8_HAVE_HW_SPI
//#include <SPI.h>
//#endif
//#ifdef U8X8_HAVE_HW_I2C
//#include <Wire.h>
//#endif

DisplayMono::DisplayMono() {
    mEnPowerSafe = true;
    mEnScreenSaver = true;
    mLuminance = 60;
    _dispY = 0;
    mTimeout = DISP_DEFAULT_TIMEOUT;  // interval at which to power save (milliseconds)
    mUtcTs = NULL;
    mType = 0;
}



void DisplayMono::init(uint8_t type, uint8_t rotation, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t *utcTs, const char* version) {
    if ((0 < type) && (type < 5)) {
        u8g2_cb_t *rot = (u8g2_cb_t *)((rotation != 0x00) ? U8G2_R2 : U8G2_R0);
        mType = type;
        switch(type) {
            case 1:
                mDisplay = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(rot, reset, clock, data);
                break;
            default:
            case 2:
                mDisplay = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(rot, reset, clock, data);
                break;
            case 3:
                mDisplay = new U8G2_PCD8544_84X48_F_4W_SW_SPI(rot, clock, data, cs, dc, reset);
                break;
            case 4:
                mDisplay = new U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C(rot, reset, clock, data);
                break;
        }

        mUtcTs = utcTs;

        mDisplay->begin();

        mIsWide = (mDisplay->getWidth() > 120);
        mIsTall = (mDisplay->getHeight() > 60);
        calcLinePositions();

        mDisplay->clearBuffer();
        if (3 != mType)
            mDisplay->setContrast(mLuminance);
        printText("AHOY!", 0);
        printText("ahoydtu.de", 2);
        printText(version, 3);
        mDisplay->sendBuffer();
    }
}

void DisplayMono::config(bool enPowerSafe, bool enScreenSaver, uint8_t lum) {
    mEnPowerSafe = enPowerSafe;
    mEnScreenSaver = enScreenSaver;
    mLuminance = lum;
}

void DisplayMono::loop(void) {
    if (mEnPowerSafe)
        if(mTimeout != 0)
           mTimeout--;
}

void DisplayMono::disp(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod) {


    mDisplay->clearBuffer();

    // set Contrast of the Display to raise the lifetime
    if (3 != mType)
        mDisplay->setContrast(mLuminance);

    if ((totalPower > 0) && (isprod > 0)) {
        mTimeout = DISP_DEFAULT_TIMEOUT;
        mDisplay->setPowerSave(false);
        if (totalPower > 999) {
            snprintf(_fmtText, DISP_FMT_TEXT_LEN, "%2.2f kW", (totalPower / 1000));
        } else {
            snprintf(_fmtText, DISP_FMT_TEXT_LEN, "%3.0f W", totalPower);
        }
        printText(_fmtText, 0);
    } else {
        printText("offline", 0);
        // check if it's time to enter power saving mode
        if (mTimeout == 0)
            mDisplay->setPowerSave(mEnPowerSafe);
    }

    snprintf(_fmtText, DISP_FMT_TEXT_LEN, "today: %4.0f Wh", totalYieldDay);
    printText(_fmtText, 1);

    snprintf(_fmtText, DISP_FMT_TEXT_LEN, "total: %.1f kWh", totalYieldTotal);
    printText(_fmtText, 2);

    IPAddress ip = WiFi.localIP();
    if (!(_mExtra % 10) && (ip)) {
        printText(ip.toString().c_str(), 3);
    } else if (!(_mExtra % 5)) {
        snprintf(_fmtText, DISP_FMT_TEXT_LEN, "%d Inverter on", isprod);
        printText(_fmtText, 3);
    } else {
        if (NULL != mUtcTs){
            if(mIsWide && mIsTall)
                printText(ah::getDateTimeStr(gTimezone.toLocal(*mUtcTs)).c_str(), 3);
            else
                printText(ah::getTimeStr(gTimezone.toLocal(*mUtcTs)).c_str(), 3);
        }
    }

    mDisplay->sendBuffer();

    _dispY = 0;
    _mExtra++;
}

void DisplayMono::calcLinePositions() {
    uint8_t yOff[] = {0,0};
    for (uint8_t i = 0; i < 4; i++) {
        setFont(i);
        yOff[getColumn(i)] += (mDisplay->getMaxCharHeight());
        mLineYOffsets[i] = yOff[getColumn(i)];
        if (isTwoRowLine(i)){
          yOff[getColumn(i)] += mDisplay->getMaxCharHeight();
        }
        yOff[getColumn(i)]+= BOTTOM_MARGIN;
        if (mIsTall && mIsWide){
          mLineXOffsets[i] = (i == 0) ? 10 : 5 + (getColumn(i)==1? 80 : 0);
        } else {
          mLineXOffsets[i] = (getColumn(i)==1? 80 : 0);
        }
    }
}

inline void DisplayMono::setFont(uint8_t line) {
    switch (line) {
        case 0:
            if (mIsWide && mIsTall){
                mDisplay->setFont(u8g2_font_ncenB14_tr);
            } else if (mIsWide && ! mIsTall){
                mDisplay->setFont(u8g2_font_9x15_tf);
            } else {
                mDisplay->setFont(u8g2_font_logisoso16_tr);
            }
            break;
        case 3:
            if  (mIsWide && ! mIsTall){
                mDisplay->setFont(u8g2_font_tom_thumb_4x6_tf);
            } else {
                mDisplay->setFont(u8g2_font_5x8_tr);
            }
            break;
        default:
            if (mIsTall){
                mDisplay->setFont(u8g2_font_ncenB10_tr);
            } else if (mIsWide){
                mDisplay->setFont(u8g2_font_tom_thumb_4x6_tf);
            } else {
                mDisplay->setFont(u8g2_font_5x8_tr);
            }
            break;
    }
}

inline uint8_t DisplayMono::getColumn(uint8_t line) {
    if (mIsTall){
        return 0;
    } else if (line>=1 && line<=2){
        return 1;
    } else {
        return 0;
    }
}

inline bool DisplayMono::isTwoRowLine(uint8_t line) {
    if (mIsTall){
        return false;
    } else if (line>=1 && line<=2){
        return true;
    } else {
        return false;
    }
}
void DisplayMono::printText(const char* text, uint8_t line) {
    setFont(line);

    uint8_t dispX = mLineXOffsets[line] + ((mEnScreenSaver) ? (_mExtra % 7) : 0);

    if (isTwoRowLine(line)){
        String stringText = String(text);
        int space = stringText.indexOf(" ");
        mDisplay->drawStr(dispX, mLineYOffsets[line], stringText.substring(0,space).c_str());
        if (space>0)
            mDisplay->drawStr(dispX, mLineYOffsets[line] + mDisplay->getMaxCharHeight(), stringText.substring(space+1).c_str());
    } else {
        mDisplay->drawStr(dispX, mLineYOffsets[line], text);
    }
}


