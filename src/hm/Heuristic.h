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

#define RF_TX_CHAN_QUALITY_GOOD         2
#define RF_TX_CHAN_QUALITY_OK           1
#define RF_TX_CHAN_QUALITY_LOW          -1
#define RF_TX_CHAN_QUALITY_BAD          -2

class Heuristic {
    public:
        uint8_t getTxCh(Inverter<> *iv) {
            if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen))
                return 0; // not used for these inverter types

            HeuristicInv *ih = &iv->heuristics;

            // start with the next index: round robbin in case of same 'best' quality
            uint8_t curId = (ih->txRfChId + 1) % RF_MAX_CHANNEL_ID;
            ih->lastBestTxChId = ih->txRfChId;
            ih->txRfChId = curId;
            curId = (curId + 1) % RF_MAX_CHANNEL_ID;
            for(uint8_t i = 1; i < RF_MAX_CHANNEL_ID; i++) {
                if(ih->txRfQuality[curId] > ih->txRfQuality[ih->txRfChId])
                    ih->txRfChId = curId;
                curId = (curId + 1) % RF_MAX_CHANNEL_ID;
            }

            if(ih->testPeriodSendCnt < 0xff)
                ih->testPeriodSendCnt++;

            if((ih->txRfChId == ih->lastBestTxChId) && (ih->testPeriodSendCnt >= RF_TEST_PERIOD_MAX_SEND_CNT)) {
                if(ih->testPeriodFailCnt > RF_TEST_PERIOD_MAX_FAIL_CNT) {
                    // try round robbin another chan and see if it works even better
                    ih->testChId = (ih->testChId + 1) % RF_MAX_CHANNEL_ID;
                    if(ih->testChId == ih->txRfChId)
                        ih->testChId = (ih->testChId + 1) % RF_MAX_CHANNEL_ID;

                    // give it a fair chance but remember old status in case of immediate fail
                    ih->saveOldTestQuality = ih->txRfQuality[ih->testChId];
                    ih->txRfQuality[ih->testChId] = ih->txRfQuality[ih->txRfChId];
                    ih->txRfChId = ih->testChId;
                    ih->testChId = RF_TX_TEST_CHAN_1ST_USE; // mark the chan as a test and as 1st use during new test period
                    DPRINTLN(DBG_INFO, "Test CH " + String(id2Ch(ih->txRfChId)));
                }

                // start new test period
                ih->testPeriodSendCnt = 0;
                ih->testPeriodFailCnt = 0;
            } else if(ih->txRfChId != ih->lastBestTxChId) {
                // start new test period
                ih->testPeriodSendCnt = 0;
                ih->testPeriodFailCnt = 0;
            }

            return id2Ch(ih->txRfChId);
        }

        void evalTxChQuality(Inverter<> *iv, bool crcPass, uint8_t retransmits, uint8_t rxFragments) {
            HeuristicInv *ih = &iv->heuristics;

            if(ih->lastRxFragments == rxFragments) {
                // nothing received: send probably lost
                if(!retransmits || isNewTxCh(ih)) {
                    if(RF_TX_TEST_CHAN_1ST_USE == ih->testChId) {
                        // switch back to original quality
                        ih->txRfQuality[ih->txRfChId] = ih->saveOldTestQuality;

                        updateQuality(ih, RF_TX_CHAN_QUALITY_BAD);
                        if(ih->testPeriodFailCnt < 0xff)
                            ih->testPeriodFailCnt++;
                    }
                }
            } else if(!ih->lastRxFragments && crcPass) {
                if(!retransmits || isNewTxCh(ih)) {
                    // every fragment received successfull immediately
                    updateQuality(ih, RF_TX_CHAN_QUALITY_GOOD);
                } else {
                    // every fragment received successfully
                    updateQuality(ih, RF_TX_CHAN_QUALITY_OK);
                }
            } else if(crcPass) {
                if(isNewTxCh(ih)) {
                    // last Fragment successfully received on new send channel
                    updateQuality(ih, RF_TX_CHAN_QUALITY_OK);
                }
            } else if(!retransmits || isNewTxCh(ih)) {
                // no complete receive for this send channel
                if((rxFragments - ih->lastRxFragments) > 2) {
                    // graceful evaluation for big inverters that have to send 4 answer packets
                    updateQuality(ih, RF_TX_CHAN_QUALITY_OK);
                } else if((rxFragments - ih->lastRxFragments) < 2) {
                    if(RF_TX_TEST_CHAN_1ST_USE == ih->testChId) {
                        // switch back to original quality
                        ih->txRfQuality[ih->txRfChId] = ih->saveOldTestQuality;
                    }
                    updateQuality(ih, RF_TX_CHAN_QUALITY_LOW);
                    if(ih->testPeriodFailCnt < 0xff)
                        ih->testPeriodFailCnt++;
                } // else: _QUALITY_NEUTRAL, keep any test channel
            } // else: dont overestimate burst distortion

            ih->testChId = ih->txRfChId; // reset to best
            ih->lastRxFragments = rxFragments;
        }

        void printStatus(Inverter<> *iv) {
            DPRINT_IVID(DBG_INFO, iv->id);
            DBGPRINT(F("Radio infos:"));
            if((IV_HMS != iv->ivGen) && (IV_HMT != iv->ivGen)) {
                for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                    DBGPRINT(F(" "));
                    DBGPRINT(String(iv->heuristics.txRfQuality[i]));
                }
                DBGPRINT(F(" |"));
            }
            DBGPRINT(F(" t: "));
            DBGPRINT(String(iv->radioStatistics.txCnt));
            DBGPRINT(F(", s: "));
            DBGPRINT(String(iv->radioStatistics.rxSuccess));
            DBGPRINT(F(", f: "));
            DBGPRINT(String(iv->radioStatistics.rxFail));
            DBGPRINT(F(", n: "));
            DBGPRINT(String(iv->radioStatistics.rxFailNoAnser));
            DBGPRINT(F(" | p: "));                                  // better debugging for helpers...
            if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen))
                DBGPRINTLN(String(iv->config->powerLevel-10));
            else
                DBGPRINTLN(String(iv->config->powerLevel));
        }

    private:
        bool isNewTxCh(HeuristicInv *ih) {
            return ih->txRfChId != ih->lastBestTxChId;
        }

        void updateQuality(HeuristicInv *ih, uint8_t quality) {
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
