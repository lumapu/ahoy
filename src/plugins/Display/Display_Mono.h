//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include <U8g2lib.h>
#define DISP_DEFAULT_TIMEOUT 60  // in seconds
#define DISP_FMT_TEXT_LEN 32
#define BOTTOM_MARGIN 5

#include "defines.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "../../utils/helper.h"
#include "Display_data.h"

class DisplayMono {
   public:
      DisplayMono() {};

      virtual void init(uint8_t type, uint8_t rot, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, DisplayData *displayData) = 0;
      virtual void config(bool enPowerSave, bool enScreenSaver, uint8_t lum) = 0;
      virtual void loop(uint8_t lum) = 0;
      virtual void disp(void) = 0;

   protected:
      U8G2* mDisplay;
      DisplayData *mDisplayData;

      uint8_t mType;
      uint16_t mDispWidth;
      uint16_t mDispHeight;

      bool mEnPowerSave, mEnScreenSaver;
      uint8_t mLuminance;

      uint8_t mLoopCnt;
      uint8_t mLineXOffsets[5] = {};
      uint8_t mLineYOffsets[5] = {};

      uint16_t mDispY;

      uint8_t mExtra;
      uint16_t mTimeout;
      char mFmtText[DISP_FMT_TEXT_LEN];

      // Common initialization function to be called by subclasses
      void monoInit(U8G2* display, uint8_t type, DisplayData *displayData) {
         mDisplay = display;
         mType = type;
         mDisplayData = displayData;
         mDisplay->begin();
         mDisplay->setContrast(mLuminance);
         mDisplay->clearBuffer();
         mDispWidth = mDisplay->getDisplayWidth();
         mDispHeight = mDisplay->getDisplayHeight();
      }
};