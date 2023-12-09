//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HEURISTIC_H__
#define __HEURISTIC_H__

#include "../utils/dbg.h"
#include "hmInverter.h"
#include "HeuristicInv.h"

#define RF_TEST_PERIOD_MAX_SEND_CNT     50
#define RF_TEST_PERIOD_MAX_FAIL_CNT     5

#define RF_TX_TEST_CHAN_1ST_USE         0xff

class Heuristic {
    public:
        uint8_t getTxCh(Inverter<> *iv) {
            if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen))
                return 0; // not used for these inverter types

            HeuristicInv *ih = &iv->heuristics;

            // start with the next index: round robbin in case of same 'best' quality
            uint8_t curId = (ih->txRfChId + 1) % RF_MAX_CHANNEL_ID;
            uint8_t lastBestId = ih->txRfChId;
            ih->txRfChId = curId;
            curId = (curId + 1) % RF_MAX_CHANNEL_ID;
            for(uint8_t i = 1; i < RF_MAX_CHANNEL_ID; i++) {
                if(ih->txRfQuality[curId] > ih->txRfQuality[ih->txRfChId])
                    ih->txRfChId = curId;
                curId = (curId + 1) % RF_MAX_CHANNEL_ID;
            }

            if(ih->testPeriodSendCnt < 0xff)
                ih->testPeriodSendCnt++;

            if((ih->txRfChId == lastBestId) && (ih->testPeriodSendCnt >= RF_TEST_PERIOD_MAX_SEND_CNT)) {
                if(ih->testPeriodFailCnt > RF_TEST_PERIOD_MAX_FAIL_CNT) {
                     // try round robbin another chan and see if it works even better
                    ih->testChId = (ih->testChId + 1) % RF_MAX_CHANNEL_ID;
                    if(ih->testChId = ih->txRfChId)
                        ih->testChId = (ih->testChId + 1) % RF_MAX_CHANNEL_ID;

                    // give it a fair chance but remember old status in case of immediate fail
                    ih->txRfChId = ih->testChId;
                    ih->testChId = RF_TX_TEST_CHAN_1ST_USE; // mark the chan as a test and as 1st use during new test period
                    DPRINTLN(DBG_INFO, "Test CH " + String(id2Ch(ih->txRfChId)));
                }

                // start new test period
                ih->testPeriodSendCnt = 0;
                ih->testPeriodFailCnt = 0;
            } else if(ih->txRfChId != lastBestId) {
                // start new test period
                ih->testPeriodSendCnt = 0;
                ih->testPeriodFailCnt = 0;
            }

            return id2Ch(ih->txRfChId);
        }

        void setGotAll(Inverter<> *iv) {
            updateQuality(iv, 2); // GOOD
        }

        void setGotFragment(Inverter<> *iv) {
            updateQuality(iv, 1); // OK
        }

        void setGotNothing(Inverter<> *iv) {
            HeuristicInv *ih = &iv->heuristics;

            if(RF_TX_TEST_CHAN_1ST_USE == ih->testChId) {
                // immediate fail
                ih->testChId = ih->txRfChId; // reset to best
                return;
            }

            if(ih->testPeriodFailCnt < 0xff)
                ih->testPeriodFailCnt++;

            updateQuality(iv, -2); // BAD
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

    private:
        void updateQuality(Inverter<> *iv, uint8_t quality) {
            HeuristicInv *ih = &iv->heuristics;

            ih->txRfQuality[ih->txRfChId] += quality;
            if(ih->txRfQuality[ih->txRfChId] > RF_MAX_QUALITY)
                ih->txRfQuality[ih->txRfChId] = RF_MAX_QUALITY;
            else if(ih->txRfQuality[ih->txRfChId] < RF_MIN_QUALTIY)
                ih->txRfQuality[ih->txRfChId] = RF_MIN_QUALTIY;
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
