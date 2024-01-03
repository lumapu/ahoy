//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HEURISTIC_INV_H__
#define __HEURISTIC_INV_H__

#define RF_MAX_CHANNEL_ID   5
#define RF_MAX_QUALITY      4
#define RF_MIN_QUALTIY      -6
#define RF_NA               -99

class HeuristicInv {
    public:
        HeuristicInv() {
            memset(txRfQuality, -6, RF_MAX_CHANNEL_ID);
        }

    public:
        int8_t  txRfQuality[RF_MAX_CHANNEL_ID]; // heuristics tx quality (check 'Heuristics.h')
        uint8_t txRfChId       = 0;             // RF TX channel id
        uint8_t lastBestTxChId = 0;

        uint8_t testPeriodSendCnt  = 0;
        uint8_t testPeriodFailCnt  = 0;
        uint8_t testChId           = 0;
        int8_t  saveOldTestQuality = -6;
        uint8_t lastRxFragments    = 0;
};

#endif /*__HEURISTIC_INV_H__*/
