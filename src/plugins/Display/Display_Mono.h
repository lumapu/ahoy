//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#pragma once
#include "config/config.h"
#include <U8g2lib.h>
#define DISP_DEFAULT_TIMEOUT 60  // in seconds
#define DISP_FMT_TEXT_LEN 32
#define BOTTOM_MARGIN 5


#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "../../utils/helper.h"

class DisplayMono {
   public:
      DisplayMono() {};

      virtual void init(uint8_t type, uint8_t rot, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t* utcTs, const char* version) = 0;
      virtual void config(bool enPowerSafe, bool enScreenSaver, uint8_t lum) = 0;
      virtual void loop(void) = 0;
      virtual void disp(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod) = 0;

   protected:
      U8G2* mDisplay;

      uint8_t mType;
      bool mEnPowerSafe, mEnScreenSaver;
      uint8_t mLuminance;

      uint8_t mLoopCnt;
      uint32_t* mUtcTs;
      uint8_t mLineXOffsets[5] = {};
      uint8_t mLineYOffsets[5] = {};

      uint16_t mDispY;

      uint8_t mExtra;
      uint16_t mTimeout;
      char mFmtText[DISP_FMT_TEXT_LEN];

#ifdef DISPLAY_CHART
#define DISP_WATT_ARR_LENGTH 128		   // Number of WATT history values
		float m_wattArr[DISP_WATT_ARR_LENGTH + 1]; // ring buffer for watt history
		int m_wattListIdx;						   // index for next Element to write into WattArr
		int m_wattDispIdx;						   // index for 1st Element to display from WattArr
		void drawPowerChart();
#endif
};