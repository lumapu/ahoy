#include "Display_ePaper.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "../../utils/helper.h"
#include "imagedata.h"

#if defined(ESP32)

static const uint32_t spiClk = 4000000;  // 4 MHz

#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
SPIClass hspi(HSPI);
#endif

DisplayEPaper::DisplayEPaper() {
    mDisplayRotation = 2;
    mHeadFootPadding = 16;
}

//***************************************************************************
void DisplayEPaper::init(uint8_t type, uint8_t _CS, uint8_t _DC, uint8_t _RST, uint8_t _BUSY, uint8_t _SCK, uint8_t _MOSI, uint32_t *utcTs, const char *version) {
    mUtcTs = utcTs;

    if (type == 10) {
        Serial.begin(115200);
        _display = new GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>(GxEPD2_150_BN(_CS, _DC, _RST, _BUSY));

#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
        hspi.begin(_SCK, _BUSY, _MOSI, _CS);
        _display->epd2.selectSPI(hspi, SPISettings(spiClk, MSBFIRST, SPI_MODE0));
#elif defined(ESP32)
        _display->epd2.init(_SCK, _MOSI, 115200, true, 20, false);
#endif
        _display->init(115200, true, 20, false);
        _display->setRotation(mDisplayRotation);
        _display->setFullWindow();

        // Logo
        _display->fillScreen(GxEPD_BLACK);
        _display->drawBitmap(0, 0, logo, 200, 200, GxEPD_WHITE);
        while (_display->nextPage())
            ;

        // clean the screen
        delay(2000);
        _display->fillScreen(GxEPD_WHITE);
        while (_display->nextPage())
            ;

        headlineIP();

        // call the PowerPage to change the PV Power Values
        actualPowerPaged(0, 0, 0, 0);
    }
}

void DisplayEPaper::config(uint8_t rotation, bool enPowerSafe) {
    mDisplayRotation = rotation;
    mEnPowerSafe = enPowerSafe;
}

//***************************************************************************
void DisplayEPaper::fullRefresh() {
    // screen complete black
    _display->fillScreen(GxEPD_BLACK);
    while (_display->nextPage())
        ;
    delay(2000);
    // screen complete white
    _display->fillScreen(GxEPD_WHITE);
    while (_display->nextPage())
        ;
}
//***************************************************************************
void DisplayEPaper::headlineIP() {
    int16_t tbx, tby;
    uint16_t tbw, tbh;

    _display->setFont(&FreeSans9pt7b);
    _display->setTextColor(GxEPD_WHITE);

    _display->setPartialWindow(0, 0, _display->width(), mHeadFootPadding);
    _display->fillScreen(GxEPD_BLACK);

    do {
        if ((WiFi.isConnected() == true) && (WiFi.localIP() > 0)) {
            snprintf(_fmtText, sizeof(_fmtText), "%s", WiFi.localIP().toString().c_str());
        } else {
            snprintf(_fmtText, sizeof(_fmtText), "WiFi not connected");
        }
        _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
        uint16_t x = ((_display->width() - tbw) / 2) - tbx;

        _display->setCursor(x, (mHeadFootPadding - 2));
        _display->println(_fmtText);
    } while (_display->nextPage());
}
//***************************************************************************
void DisplayEPaper::lastUpdatePaged() {
    int16_t tbx, tby;
    uint16_t tbw, tbh;

    _display->setFont(&FreeSans9pt7b);
    _display->setTextColor(GxEPD_WHITE);

    _display->setPartialWindow(0, _display->height() - mHeadFootPadding, _display->width(), mHeadFootPadding);
    _display->fillScreen(GxEPD_BLACK);
    do {
        if (NULL != mUtcTs) {
            snprintf(_fmtText, sizeof(_fmtText), ah::getDateTimeStr(gTimezone.toLocal(*mUtcTs)).c_str());

            _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
            uint16_t x = ((_display->width() - tbw) / 2) - tbx;

            _display->setCursor(x, (_display->height() - 3));
            _display->println(_fmtText);
        }
    } while (_display->nextPage());
}
//***************************************************************************
void DisplayEPaper::offlineFooter() {
    int16_t tbx, tby;
    uint16_t tbw, tbh;

    _display->setFont(&FreeSans9pt7b);
    _display->setTextColor(GxEPD_WHITE);

    _display->setPartialWindow(0, _display->height() - mHeadFootPadding, _display->width(), mHeadFootPadding);
    _display->fillScreen(GxEPD_BLACK);
    do {
        if (NULL != mUtcTs) {
            snprintf(_fmtText, sizeof(_fmtText), "offline");

            _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
            uint16_t x = ((_display->width() - tbw) / 2) - tbx;

            _display->setCursor(x, (_display->height() - 3));
            _display->println(_fmtText);
        }
    } while (_display->nextPage());
}
//***************************************************************************
void DisplayEPaper::actualPowerPaged(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod) {
    int16_t tbx, tby;
    uint16_t tbw, tbh, x, y;

    _display->setFont(&FreeSans24pt7b);
    _display->setTextColor(GxEPD_BLACK);

    _display->setPartialWindow(0, mHeadFootPadding, _display->width(), _display->height() - (mHeadFootPadding * 2));
    _display->fillScreen(GxEPD_WHITE);

    do {
        // actual Production
        if (totalPower > 9999) {
            snprintf(_fmtText, sizeof(_fmtText), "%.1f kW", (totalPower / 1000));
            _changed = true;
        } else if ((totalPower > 0) && (totalPower <= 9999)) {
            snprintf(_fmtText, sizeof(_fmtText), "%.0f W", totalPower);
            _changed = true;
        } else {
            snprintf(_fmtText, sizeof(_fmtText), "offline");
        }
        _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
        x = ((_display->width() - tbw) / 2) - tbx;
        _display->setCursor(x, mHeadFootPadding + tbh + 10);
        _display->print(_fmtText);

        if ((totalYieldDay > 0) && (totalYieldTotal > 0)) {
            // Today Production
            _display->setFont(&FreeSans18pt7b);
            y = _display->height() / 2;
            _display->setCursor(5, y);

            if (totalYieldDay > 9999) {
                snprintf(_fmtText, _display->width(), "%.1f", (totalYieldDay / 1000));
                _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
                _display->drawInvertedBitmap(5, y - ((tbh + 30) / 2), myToday, 30, 30, GxEPD_BLACK);
                x = ((_display->width() - tbw - 20) / 2) - tbx;
                _display->setCursor(x, y);
                _display->print(_fmtText);
                _display->setCursor(_display->width() - 50, y);
                _display->setFont(&FreeSans12pt7b);
                _display->println("kWh");
            } else if (totalYieldDay <= 9999) {
                snprintf(_fmtText, _display->width(), "%.0f", (totalYieldDay));
                _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
                _display->drawInvertedBitmap(5, y - tbh, myToday, 30, 30, GxEPD_BLACK);
                x = ((_display->width() - tbw - 20) / 2) - tbx;
                _display->setCursor(x, y);
                _display->print(_fmtText);
                _display->setCursor(_display->width() - 38, y);
                _display->setFont(&FreeSans12pt7b);
                _display->println("Wh");
            }
            y = y + tbh + 15;

            // Total Production
            _display->setFont(&FreeSans18pt7b);
            _display->setCursor(5, y);
            if (totalYieldTotal > 9999) {
                snprintf(_fmtText, _display->width(), "%.1f", (totalYieldTotal / 1000));
                _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
                _display->drawInvertedBitmap(5, y - tbh, mySigma, 30, 30, GxEPD_BLACK);
                x = ((_display->width() - tbw - 20) / 2) - tbx;
                _display->setCursor(x, y);
                _display->print(_fmtText);
                _display->setCursor(_display->width() - 59, y);
                _display->setFont(&FreeSans12pt7b);
                _display->println("MWh");
            } else if (totalYieldTotal <= 9999) {
                snprintf(_fmtText, _display->width(), "%.0f", (totalYieldTotal));
                _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
                _display->drawInvertedBitmap(5, y - tbh, mySigma, 30, 30, GxEPD_BLACK);
                x = ((_display->width() - tbw - 20) / 2) - tbx;
                _display->setCursor(x, y);
                _display->print(_fmtText);
                _display->setCursor(_display->width() - 50, y);
                _display->setFont(&FreeSans12pt7b);
                _display->println("kWh");
            }
        }

        // Inverter online
        _display->setFont(&FreeSans12pt7b);
        y = _display->height() - (mHeadFootPadding + 10);
        snprintf(_fmtText, sizeof(_fmtText), " %d online", isprod);
        _display->getTextBounds(_fmtText, 0, 0, &tbx, &tby, &tbw, &tbh);
        _display->drawInvertedBitmap(10, y - tbh, myWR, 20, 20, GxEPD_BLACK);
        x = ((_display->width() - tbw - 20) / 2) - tbx;
        _display->setCursor(x, y);
        _display->println(_fmtText);
    } while (_display->nextPage());
}
//***************************************************************************
void DisplayEPaper::loop(float totalPower, float totalYieldDay, float totalYieldTotal, uint8_t isprod) {
    // check if the IP has changed
    if (_settedIP != WiFi.localIP().toString().c_str()) {
        // save the new IP and call the Headline Function to adapt the Headline
        _settedIP = WiFi.localIP().toString().c_str();
        headlineIP();
    }

    // call the PowerPage to change the PV Power Values
    actualPowerPaged(totalPower, totalYieldDay, totalYieldTotal, isprod);

    // if there was an change and the Inverter is producing set a new Timestamp in the footline
    if ((isprod > 0) && (_changed)) {
        _changed = false;
        lastUpdatePaged();
    } else if ((0 == totalPower) && (mEnPowerSafe))
        offlineFooter();

    _display->powerOff();
}
//***************************************************************************
#endif  // ESP32
