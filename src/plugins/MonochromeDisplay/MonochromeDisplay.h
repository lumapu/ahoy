#ifndef __MONOCHROME_DISPLAY__
#define __MONOCHROME_DISPLAY__

#if defined(ENA_NOKIA) || defined(ENA_SSD1306)
#ifdef ENA_NOKIA
    #include <U8g2lib.h>
    #define DISP_PROGMEM U8X8_PROGMEM
#else // ENA_SSD1306
    /* esp8266 : SCL = 5, SDA = 4 */
    /* ewsp32  : SCL = 22, SDA = 21 */
    #include <Wire.h>
    #include <SSD1306Wire.h>
    #define DISP_PROGMEM PROGMEM
#endif


#include "../../utils/helper.h"
#include "../../hm/hmSystem.h"

static uint8_t bmp_arrow[] DISP_PROGMEM = {
    B00000000, B00011100, B00011100, B00001110, B00001110, B11111110, B01111111,
    B01110000, B01110000, B00110000, B00111000, B00011000, B01111111, B00111111,
    B00011110, B00001110, B00000110, B00000000, B00000000, B00000000, B00000000} ;

template<class HMSYSTEM>
class MonochromeDisplay {
    public:
        #if defined(ENA_NOKIA)
        MonochromeDisplay() : mDisplay(U8G2_R0,5,4,16) {
            mNewPayload = false;
            mExtra      = 0;
        }
        #else // ENA_SSD1306
        MonochromeDisplay() : mDisplay(0x3c, SDA, SCL) {
            mNewPayload = false;
            mExtra      = 0;
            mRx         = 50;
            mUp         = 1;
        }
        #endif

        void setup(HMSYSTEM *sys, uint32_t *utcTs) {
            mSys   = sys;
            mUtcTs = utcTs;
            #if defined(ENA_NOKIA)
                mDisplay.begin();
                ShowInfoText("booting...");
            #else
                mDisplay.init();
                mDisplay.flipScreenVertically();
                mDisplay.setContrast(63);
                mDisplay.setBrightness(63);

                mDisplay.clear();
                mDisplay.setFont(ArialMT_Plain_24);
                mDisplay.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);

                mDisplay.drawString(64,22,"Starting...");
                mDisplay.display();
                mDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
            #endif
        }

        void loop(void) {

        }

        void payloadEventListener(uint8_t cmd) {
            mNewPayload = true;
        }

        void tickerSecond() {
            if(mNewPayload) {
                mNewPayload = false;
                DataScreen();
            }
        }

    private:
        #if defined(ENA_NOKIA)
        void ShowInfoText(const char *txt) {
            /* u8g2_font_open_iconic_embedded_2x_t 'D' + 'G' + 'J' */
            mDisplay.clear();
            mDisplay.firstPage();
            do {
                const char *e;
                const char *p = txt;
                int y=10;
                mDisplay.setFont(u8g2_font_5x8_tr);
                while(1) {
                    for(e=p+1; (*e && (*e != '\n')); e++);
                    size_t len=e-p;
                    mDisplay.setCursor(2,y);
                    String res=((String)p).substring(0,len);
                    mDisplay.print(res);
                    if ( !*e )
                        break;
                    p=e+1;
                    y+=12;
                }
                mDisplay.sendBuffer();
            } while( mDisplay.nextPage() );
        }
        #endif

        void DataScreen(void) {
            String timeStr = ah::getDateTimeStr(*mUtcTs).substring(2, 22);
            IPAddress ip = WiFi.localIP();
            float totalYield = 0.000, totalYieldToday = 0.000, totalActual = 0.0;
            char fmtText[32];
            int  ucnt=0;

            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL != iv) {
                    record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                    uint8_t pos;
                    uint8_t list[] = {FLD_PAC, FLD_YT, FLD_YD};

                    if ( !iv->isProducing(*mUtcTs,rec) )
                        continue;

                    ucnt++;

                    for (uint8_t fld = 0; fld < 3; fld++) {
                        pos = iv->getPosByChFld(CH0, list[fld],rec);

                        if(fld == 1)
                            totalYield += iv->getValue(pos,rec);
                        if(fld == 2)
                            totalYieldToday += iv->getValue(pos,rec);
                        if(fld == 0)
                            totalActual += iv->getValue(pos,rec);
                    }
                }
            }

            /* u8g2_font_open_iconic_embedded_2x_t 'D' + 'G' + 'J' */
            mDisplay.clear();
            #if defined(ENA_NOKIA)
                mDisplay.firstPage();
                do {
                    if(ucnt) {
                        mDisplay.drawXBMP(10,0,8,17,bmp_arrow);
                        mDisplay.setFont(u8g2_font_logisoso16_tr);
                        mDisplay.setCursor(25,16);
                        sprintf(fmtText,"%3.0f",totalActual);
                        mDisplay.print(String(fmtText)+F(" W"));
                        mDisplay.drawHLine(2,20,78);
                        mDisplay.setFont(u8g2_font_5x8_tr);
                        mDisplay.setCursor(5,29);
                        sprintf(fmtText,"%4.0f",totalYieldToday);
                        mDisplay.print(F("today ")+String(fmtText)+F(" Wh"));
                        mDisplay.setCursor(5,37);
                        sprintf(fmtText,"%.1f",totalYield);
                        mDisplay.print(F("total ")+String(fmtText)+F(" kWh"));
                    }
                    else {
                        mDisplay.setFont(u8g2_font_logisoso16_tr);
                        mDisplay.setCursor(30,30);
                        mDisplay.print(F("off"));
                        mDisplay.setFont(u8g2_font_5x8_tr);
                    }
                    if ( !(mExtra%20) ) {
                        mDisplay.setCursor(5,57);
                        mDisplay.print(ip);
                    }
                    else {
                        mDisplay.setCursor(0,47);
                        mDisplay.print(timeStr);
                    }

                    mDisplay.sendBuffer();
                } while( mDisplay.nextPage() );
                mExtra++;
        #else // ENA_SSD1306
            if(ucnt) {
                mDisplay.setBrightness(63);
                mDisplay.drawXbm(10,5,8,17,bmp_arrow);
                mDisplay.setFont(ArialMT_Plain_24);
                sprintf(fmtText,"%3.0f",totalActual);
                mDisplay.drawString(25,0,String(fmtText)+F(" W"));
                mDisplay.setFont(ArialMT_Plain_16);
                sprintf(fmtText,"%4.0f",totalYieldToday);
                mDisplay.drawString(5,22,F("today ")+String(fmtText)+F(" Wh"));
                sprintf(fmtText,"%.1f",totalYield);
                mDisplay.drawString(5,35,F("total  ")+String(fmtText)+F(" kWh"));
                mDisplay.drawLine(2,23,123,23);
            }
            else {
                if(mUp) {
                    mRx += 2;
                    if(mRx >= 70)
                    mUp = 0;
                } else {
                    mRx -= 2;
                    if(mRx <= 50)
                    mUp = 1;
                }
                mDisplay.setBrightness(1);
                mDisplay.setFont(ArialMT_Plain_24);
                mDisplay.drawString(mRx, 10, F("off"));
                mDisplay.setFont(ArialMT_Plain_16);
            }
            if (!(mExtra % 20)) {
                mDisplay.drawString(5, 49, ip.toString());
            } else {
                int w = mDisplay.getStringWidth(timeStr.c_str(), timeStr.length(), 0);
                if (w > 127) {
                    String tt=timeStr.substring(9, 17);
                    w=mDisplay.getStringWidth(tt.c_str(),tt.length(), 0);
                    mDisplay.drawString(127 - w, 49, tt);
                } else
                    mDisplay.drawString(0, 49, timeStr);
            }

            mDisplay.display();
            mExtra++;
        #endif
        }

        // private member variables
        #if defined(ENA_NOKIA)
            U8G2_PCD8544_84X48_1_4W_HW_SPI mDisplay;
        #else // ENA_SSD1306
            SSD1306Wire mDisplay;
            int mRx;
            char mUp;
        #endif
        int mExtra;
        bool mNewPayload;
        uint32_t *mUtcTs;
        HMSYSTEM *mSys;
};
#endif

#endif /*__MONOCHROME_DISPLAY__*/
