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
        int8_t  txRfQuality[5]; // heuristics tx quality (check 'Heuristics.h')
        uint8_t txRfChId;       // RF TX channel id

        bool testEn = false;
        uint8_t testIdx = 0;
        int8_t storedIdx = RF_NA;
};

#endif /*__HEURISTIC_INV_H__*/
