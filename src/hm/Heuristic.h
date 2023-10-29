//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HEURISTIC_H__
#define __HEURISTIC_H__

#include "../utils/dbg.h"

#define RF_MAX_CHANNEL_ID   5
#define RF_MAX_QUALITY      4
#define RF_MIN_QUALTIY      -6

class Heuristic {
    public:
        void setup() {
            uint8_t j;
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                mTxQuality[i] = -6; // worst
                for(j = 0; j < 10; j++) {
                    mRxCh[i][j] = 3;
                }
            }
        }

        /*uint8_t getRxCh(void) {

        }*/

        uint8_t getTxCh(void) {
            if(++mCycleCnt > RF_MAX_CHANNEL_ID) {
                bool ok = false;
                for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                    if(mTxQuality[i] > -4) {
                        ok     = true;
                        mState = States::RUNNING;
                    }
                }
                if(!ok) { // restart
                    mCycleCnt = 0;
                    mState    = States::TRAINING;
                }
            }

            if(States::TRAINING == mState) {
                mTxChId = (mTxChId + 1) % RF_MAX_CHANNEL_ID;
                return id2Ch(mTxChId);
            } else {
                int8_t bestQuality = -6;
                uint8_t id = 0;
                for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                    if((mCycleCnt % 50) == 0) {
                        if(mTxQuality[(i + mTxChId) % RF_MAX_CHANNEL_ID] != -6) {
                            id = i;
                            break;
                        }

                    }
                    if(mTxQuality[(i + mTxChId) % RF_MAX_CHANNEL_ID] == 4) {
                        id = i;
                        break;
                    }
                    if(mTxQuality[i] > bestQuality) {
                        bestQuality = mTxQuality[i];
                        id     = i;
                    }
                }
                mTxChId = id;
                return id2Ch(mTxChId);
            }
        }

        void setGotAll(void) {
            updateQuality(2); // GOOD
        }

        void setGotFragment(void) {
            updateQuality(1); // OK
        }

        void setGotNothing(void) {
            updateQuality(-2); // BAD
        }

        void printStatus(void) {
            DPRINT(DBG_INFO, F("Status: #"));
            DBGPRINT(String(mCycleCnt));
            DBGPRINT(F(" |"));
            for(uint8_t i = 0; i < RF_MAX_CHANNEL_ID; i++) {
                DBGPRINT(F(" "));
                DBGPRINT(String(mTxQuality[i]));
            }
            DBGPRINTLN("");
        }

    private:
        void updateQuality(uint8_t quality) {
            mTxQuality[mTxChId] += quality;
            if(mTxQuality[mTxChId] > RF_MAX_QUALITY)
                mTxQuality[mTxChId] = RF_MAX_QUALITY;
            else if(mTxQuality[mTxChId] < RF_MIN_QUALTIY)
                mTxQuality[mTxChId] = RF_MIN_QUALTIY;
        }

        uint8_t ch2Id(uint8_t ch) {
            switch(ch) {
                case 3:  return 0;
                case 23: return 1;
                case 40: return 2;
                case 61: return 3;
                case 75: return 4;
            }
            return 0; // standard
        }

        uint8_t id2Ch(uint8_t id) {
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
        enum class States : uint8_t {
            TRAINING, RUNNING
        };

    private:
        uint8_t mChList[5] = {03, 23, 40, 61, 75};

        int8_t mTxQuality[RF_MAX_CHANNEL_ID];
        uint8_t mTxChId = 0;

        uint8_t mRxCh[RF_MAX_CHANNEL_ID][10];
        uint8_t mRxChId = 0;

        uint32_t mCycleCnt = 0;
        States mState = States::TRAINING;
};


#endif /*__HEURISTIC_H__*/
