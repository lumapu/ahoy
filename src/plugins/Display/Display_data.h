#include "../../utils/helper.h"

#ifndef __DISPLAY_DATA__
#define __DISPLAY_DATA__

struct DisplayData {
        const char *version=nullptr;
        float totalPower=0.0f;
        float totalYieldDay=0.0f;
        float totalYieldTotal=0.0f;
        uint32_t utcTs=0;
        uint8_t isProducing=0;
        int8_t wifiRSSI=SCHAR_MIN;
        bool showRadioSymbol = false;
        bool WiFiSymbol = false;
        bool RadioSymbol = false;
        IPAddress ipAddress;
};

#endif /*__DISPLAY_DATA__*/
