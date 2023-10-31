//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HEURISTIC_H__
#define __HEURISTIC_H__

#include "../utils/dbg.h"
#include "hmInverter.h"

#define RF_MAX_CHANNEL_ID   5
#define RF_MAX_QUALITY      4
#define RF_MIN_QUALTIY      -6

class Heuristic {
    public:
        uint8_t getTxCh(Inverter<> *iv) {
            if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen))
                return 0; // not used for these inverter types

            uint8_t id = 0;
            int8_t bestQuality = -6;
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                if(iv->txRfQuality[i] > bestQuality) {
                    bestQuality = iv->txRfQuality[i];
                    id = i;
                }
            }
            if(bestQuality > -6)
                iv->txRfChId = (iv->txRfChId + 1) % RF_MAX_CHANNEL_ID; // next channel
            else
                iv->txRfChId = id; // best quality channel

            return id2Ch(iv->txRfChId);
        }

        void setGotAll(Inverter<> *iv) {
            updateQuality(iv, 2); // GOOD
        }

        void setGotFragment(Inverter<> *iv) {
            updateQuality(iv, 1); // OK
        }

        void setGotNothing(Inverter<> *iv) {
            updateQuality(iv, -2); // BAD
        }

        void printStatus(Inverter<> *iv) {
            DPRINT(DBG_INFO, F("Status:"));
            DBGPRINT(F(" |"));
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                DBGPRINT(F(" "));
                DBGPRINT(String(iv->txRfQuality[i]));
            }
            DBGPRINTLN("");
        }

    private:
        void updateQuality(Inverter<> *iv, uint8_t quality) {
            iv->txRfQuality[iv->txRfChId] += quality;
            if(iv->txRfQuality[iv->txRfChId] > RF_MAX_QUALITY)
                iv->txRfQuality[iv->txRfChId] = RF_MAX_QUALITY;
            else if(iv->txRfQuality[iv->txRfChId] < RF_MIN_QUALTIY)
                iv->txRfQuality[iv->txRfChId] = RF_MIN_QUALTIY;
        }

        /*uint8_t ch2Id(uint8_t ch) {
            switch(ch) {
                case 3:  return 0;
                case 23: return 1;
                case 40: return 2;
                case 61: return 3;
                case 75: return 4;
            }
            return 0; // standard
        }*/

        inline uint8_t id2Ch(uint8_t id) {
            switch(id) {
                case 0: return 3;
                case 1: return 23;
                case 2: return 40;
                case 3: return 61;
                case 4: return 75;
            }
            return 0; // standard
        }

    private:
        uint8_t mChList[5] = {03, 23, 40, 61, 75};
};


#endif /*__HEURISTIC_H__*/
