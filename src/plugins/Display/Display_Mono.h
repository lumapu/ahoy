// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <U8g2lib.h>
#define DISP_DEFAULT_TIMEOUT 60  // in seconds
#define DISP_FMT_TEXT_LEN    32
#define BOTTOM_MARGIN         5

class DisplayMono {
   public:
      DisplayMono();

      void init(uint8_t type, uint8_t rot, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t *utcTs, const char* version);
      void config(bool enPowerSafe, bool enScreenSaver, uint8_t lum);
      void loop(void);
      void disp(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod);

   private:
      void calcLinePositions();
      void setFont(uint8_t line);
      uint8_t getColumn(uint8_t line);
      bool isTwoRowLine(uint8_t line);
      void printText(const char* text, uint8_t line);

      U8G2* mDisplay;

      uint8_t mType;
      bool mEnPowerSafe, mEnScreenSaver;
      uint8_t mLuminance;

      bool mIsTall = false;
      bool mIsWide = false;
      uint8_t mLoopCnt;
      uint32_t* mUtcTs;
      uint8_t mLineXOffsets[5];
      uint8_t mLineYOffsets[5];

      uint16_t _dispY;

      uint8_t _mExtra;
      uint16_t mTimeout;
      char _fmtText[DISP_FMT_TEXT_LEN];
};
