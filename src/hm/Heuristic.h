//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HEURISTIC_H__
#define __HEURISTIC_H__

#include "../utils/dbg.h"
#include "hmInverter.h"
#include "HeuristicInv.h"

class Heuristic {
    public:
        uint8_t getTxCh(Inverter<> *iv) {
            if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen))
                return 0; // not used for these inverter types

            uint8_t bestId = 0;
            int8_t bestQuality = -6;
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                if(iv->heuristics.txRfQuality[i] > bestQuality) {
                    bestQuality = iv->heuristics.txRfQuality[i];
                    bestId = i;
                }
            }

            if(iv->heuristics.testEn) {
                DPRINTLN(DBG_INFO, F("heuristic test mode"));
                iv->heuristics.testIdx = (iv->heuristics.testIdx + 1) % RF_MAX_CHANNEL_ID;

                if (iv->heuristics.testIdx == bestId)
                    iv->heuristics.testIdx = (iv->heuristics.testIdx + 1) % RF_MAX_CHANNEL_ID;

                // test channel get's quality of best channel (maybe temporarily, see in 'setGotNothing')
                iv->heuristics.storedIdx = iv->heuristics.txRfQuality[iv->heuristics.testIdx];
                iv->heuristics.txRfQuality[iv->heuristics.testIdx] = bestQuality;

                iv->heuristics.txRfChId = iv->heuristics.testIdx;
            } else
                iv->heuristics.txRfChId = bestId;

            return id2Ch(iv->heuristics.txRfChId);
        }

        void setGotAll(Inverter<> *iv) {
            updateQuality(iv, 2); // GOOD
            iv->heuristics.testEn = false;
        }

        void setGotFragment(Inverter<> *iv) {
            updateQuality(iv, 1); // OK
            iv->heuristics.testEn = false;
        }

        void setGotNothing(Inverter<> *iv) {
            if(RF_NA != iv->heuristics.storedIdx) {
                // if communication fails on first try with temporarily good level, revert it back to its original level
                iv->heuristics.txRfQuality[iv->heuristics.txRfChId] = iv->heuristics.storedIdx;
                iv->heuristics.storedIdx = RF_NA;
            }

            if(!iv->heuristics.testEn) {
                updateQuality(iv, -2); // BAD
                iv->heuristics.testEn = true;
            } else
                iv->heuristics.testEn = false;
        }

        void printStatus(Inverter<> *iv) {
            DPRINT_IVID(DBG_INFO, iv->id);
            DBGPRINT(F("Radio infos:"));
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                DBGPRINT(F(" "));
                DBGPRINT(String(iv->heuristics.txRfQuality[i]));
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

        bool getTestModeEnabled(Inverter<> *iv) {
            return iv->heuristics.testEn;
        }

    private:
        void updateQuality(Inverter<> *iv, uint8_t quality) {
            iv->heuristics.txRfQuality[iv->heuristics.txRfChId] += quality;
            if(iv->heuristics.txRfQuality[iv->heuristics.txRfChId] > RF_MAX_QUALITY)
                iv->heuristics.txRfQuality[iv->heuristics.txRfChId] = RF_MAX_QUALITY;
            else if(iv->heuristics.txRfQuality[iv->heuristics.txRfChId] < RF_MIN_QUALTIY)
                iv->heuristics.txRfQuality[iv->heuristics.txRfChId] = RF_MIN_QUALTIY;
        }

        inline uint8_t id2Ch(uint8_t id) {
            switch(id) {
                case 0: return 3;
                case 1: return 23;
                case 2: return 40;
                case 3: return 61;
                case 4: return 75;
            }
            return 3; // standard
        }

    private:
        uint8_t mChList[5] = {03, 23, 40, 61, 75};
};


#endif /*__HEURISTIC_H__*/
