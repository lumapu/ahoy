#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "Display_Mono.h"

class DisplayMono84X48 : public DisplayMono {
   public:
    DisplayMono84X48();

    void init(uint8_t type, uint8_t rot, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t* utcTs, const char* version);
    void config(bool enPowerSafe, bool enScreenSaver, uint8_t lum);
    void loop(void);
    void disp(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod);

   private:
    void calcLineHeights();
    void setFont(uint8_t line);
    void printText(const char* text, uint8_t line);

    U8G2* mDisplay;

    uint8_t mType;
    bool mEnPowerSafe, mEnScreenSaver;
    uint8_t mLuminance;

    uint8_t mLoopCnt;
    uint32_t* mUtcTs;
    uint8_t mLineOffsets[5];

    uint16_t _dispY;

    uint8_t _mExtra;
    uint16_t mTimeout;
    char _fmtText[DISP_FMT_TEXT_LEN];
};
