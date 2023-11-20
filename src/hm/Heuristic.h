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

            mCycle++; // intended to overflow from time to time
            if(mTestEn) {
                iv->txRfChId = mCycle % RF_MAX_CHANNEL_ID;
                DPRINTLN(DBG_INFO, F("heuristic test mode"));
                return id2Ch(iv->txRfChId);
            }

            uint8_t id = 0;
            int8_t bestQuality = -6;
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                if(iv->txRfQuality[i] > bestQuality) {
                    bestQuality = iv->txRfQuality[i];
                    id = i;
                }
            }
            if(bestQuality == -6)
                iv->txRfChId = (iv->txRfChId + 1) % RF_MAX_CHANNEL_ID; // next channel
            else
                iv->txRfChId = id; // best quality channel

            return id2Ch(iv->txRfChId);
        }

        void setGotAll(Inverter<> *iv) {
            updateQuality(iv, 2); // GOOD
            mTestEn = false;
        }

        void setGotFragment(Inverter<> *iv) {
            updateQuality(iv, 1); // OK
            mTestEn = false;
        }

        void setGotNothing(Inverter<> *iv) {
            if(!mTestEn) {
                updateQuality(iv, -2); // BAD
                mTestEn = true;
                iv->txRfChId = mCycle % RF_MAX_CHANNEL_ID;
            }
        }

        void printStatus(Inverter<> *iv) {
            DPRINT_IVID(DBG_INFO, iv->id);
            DBGPRINT(F("Radio infos:"));
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                DBGPRINT(F(" "));
                DBGPRINT(String(iv->txRfQuality[i]));
            }
            DBGPRINT(F(" | t: "));
            DBGPRINT(String(iv->radioStatistics.txCnt));
            DBGPRINT(F(", s: "));
            DBGPRINT(String(iv->radioStatistics.rxSuccess));
            DBGPRINT(F(", f: "));
            DBGPRINT(String(iv->radioStatistics.rxFail));
            DBGPRINT(F(", n: "));
            DBGPRINT(String(iv->radioStatistics.rxFailNoAnser));
            DBGPRINT(F(" | p: "));                                  // better debugging for helpers...
            DBGPRINTLN(String(iv->config->powerLevel));
        }

        bool getTestModeEnabled(void) {
            return mTestEn;
        }

    private:
        void updateQuality(Inverter<> *iv, uint8_t quality) {
            iv->txRfQuality[iv->txRfChId] += quality;
            if(iv->txRfQuality[iv->txRfChId] > RF_MAX_QUALITY)
                iv->txRfQuality[iv->txRfChId] = RF_MAX_QUALITY;
            else if(iv->txRfQuality[iv->txRfChId] < RF_MIN_QUALTIY)
                iv->txRfQuality[iv->txRfChId] = RF_MIN_QUALTIY;
        }

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
        bool mTestEn = false;
        uint8_t mCycle = 0;
};


#endif /*__HEURISTIC_H__*/
