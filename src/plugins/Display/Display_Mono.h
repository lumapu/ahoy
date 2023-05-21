// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <U8g2lib.h>
#define DISP_DEFAULT_TIMEOUT 60  // in seconds
#define DISP_FMT_TEXT_LEN 32
#define BOTTOM_MARGIN 5

class DisplayMono {
   public:
    DisplayMono(){};

    virtual void init(uint8_t type, uint8_t rot, uint8_t cs, uint8_t dc, uint8_t reset, uint8_t clock, uint8_t data, uint32_t* utcTs, const char* version) = 0;
    virtual void config(bool enPowerSafe, bool enScreenSaver, uint8_t lum) = 0;
    virtual void loop(void) = 0;
    virtual void disp(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod) = 0;

   private:
};
