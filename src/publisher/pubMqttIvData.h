//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __PUB_MQTT_IV_DATA_H__
#define __PUB_MQTT_IV_DATA_H__

#include "../utils/dbg.h"
#include "../hm/hmSystem.h"
#include "pubMqttDefs.h"

typedef std::function<void(const char *subTopic, const char *payload, bool retained, uint8_t qos)> pubMqttPublisherType;

struct sendListCmdIv {
    uint8_t cmd;
    Inverter<> *iv;
    sendListCmdIv(uint8_t a, Inverter<> *i) : cmd(a), iv(i) {}
};

template<class HMSYSTEM>
class PubMqttIvData {
    public:
        void setup(HMSYSTEM *sys, uint32_t *utcTs, std::queue<sendListCmdIv> *sendList) {
            mSys          = sys;
            mUtcTimestamp = utcTs;
            mSendList     = sendList;
            mState        = IDLE;
            mZeroValues   = false;

            mRTRDataHasBeenSent = false;

            mTable[IDLE]            = &PubMqttIvData::stateIdle;
            mTable[START]           = &PubMqttIvData::stateStart;
            mTable[FIND_NXT_IV]     = &PubMqttIvData::stateFindNxtIv;
            mTable[SEND_DATA]       = &PubMqttIvData::stateSend;
            mTable[SEND_TOTALS]     = &PubMqttIvData::stateSendTotals;
        }

        void loop() {
            (this->*mTable[mState])();
            yield();
        }

        bool start(bool zeroValues = false) {
            if(IDLE != mState)
                return false;

            mZeroValues = zeroValues;
            mRTRDataHasBeenSent = false;
            mState = START;
            return true;
        }

        void setPublishFunc(pubMqttPublisherType cb) {
            mPublish = cb;
        }

    private:
        enum State {IDLE, START, FIND_NXT_IV, SEND_DATA, SEND_TOTALS, NUM_STATES};
        typedef void (PubMqttIvData::*StateFunction)();

        void stateIdle() {
            ; // nothing to do
        }

        void stateStart() {
            mLastIvId = 0;
            mTotalFound = false;
            mSendTotalYd = true;
            mAllTotalFound = true;
            if(!mSendList->empty()) {
                mCmd = mSendList->front().cmd;
                mIvSend = mSendList->front().iv;

                if((RealTimeRunData_Debug != mCmd) || !mRTRDataHasBeenSent) { // send RealTimeRunData only once
                    mSendTotals = (RealTimeRunData_Debug == mCmd);
                    memset(mTotal, 0, sizeof(float) * 4);
                    mState = FIND_NXT_IV;
                } else
                    mSendList->pop();
            } else
                mState = IDLE;
        }

        void stateFindNxtIv() {
            bool found = false;

            for (; mLastIvId < mSys->getNumInverters(); mLastIvId++) {
                mIv = mSys->getInverterByPos(mLastIvId);
                if (NULL != mIv) {
                    if (mIv->config->enabled) {
                        found = true;
                        break;
                    }
                }
            }

            mLastIvId++;

            mPos = 0;
            if(found) {
                record_t<> *rec = mIv->getRecordStruct(mCmd);
                if(MqttSentStatus::NEW_DATA == rec->mqttSentStatus) {
                    snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/last_success", mIv->config->name);
                    snprintf(mVal, 40, "%d", mIv->getLastTs(rec));
                    mPublish(mSubTopic, mVal, true, QOS_0);

                    if((mIv->ivGen == IV_HMS) || (mIv->ivGen == IV_HMT)) {
                        snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/ch0/rssi", mIv->config->name);
                        snprintf(mVal, 40, "%d", mIv->rssi);
                        mPublish(mSubTopic, mVal, false, QOS_0);
                    }
                    rec->mqttSentStatus = MqttSentStatus::LAST_SUCCESS_SENT;
                }

                mIv->isProducing(); // recalculate status
                mState = SEND_DATA;
            } else if(mSendTotals && mTotalFound) {
                mState = SEND_TOTALS;
            } else {
                mSendList->pop();
                mZeroValues = false;
                mState = START;
            }
        }

        void stateSend() {
            record_t<> *rec = mIv->getRecordStruct(mCmd);
            if(rec == NULL) {
                if (mCmd != GetLossRate)
                    DPRINT(DBG_WARN, "unknown record to publish!");
                return;
            }

            if(mPos < rec->length) {
                bool retained = false;
                if (mCmd == RealTimeRunData_Debug) {
                    if((FLD_YT == rec->assign[mPos].fieldId) || (FLD_YD == rec->assign[mPos].fieldId))
                        retained = true;

                    // calculate total values for RealTimeRunData_Debug
                    if (CH0 == rec->assign[mPos].ch) {
                        if(mIv->getStatus() != InverterStatus::OFF) {
                            if(mIv->config->add2Total) {
                                mTotalFound = true;
                                switch (rec->assign[mPos].fieldId) {
                                    case FLD_PAC:
                                        mTotal[0] += mIv->getValue(mPos, rec);
                                        break;
                                    case FLD_YT:
                                        mTotal[1] += mIv->getValue(mPos, rec);
                                        break;
                                    case FLD_YD: {
                                        float val = mIv->getValue(mPos, rec);
                                        if(0 == val) // inverter restarted during day
                                            mSendTotalYd = false;
                                        else
                                            mTotal[2] += val;
                                        break;
                                    }
                                    case FLD_PDC:
                                        mTotal[3] += mIv->getValue(mPos, rec);
                                        break;
                                }
                            }
                        } else
                            mAllTotalFound = false;
                    }
                }

                if (MqttSentStatus::LAST_SUCCESS_SENT == rec->mqttSentStatus) {
                    uint8_t qos = (FLD_ACT_ACTIVE_PWR_LIMIT == rec->assign[mPos].fieldId) ? QOS_2 : QOS_0;
                    snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/ch%d/%s", mIv->config->name, rec->assign[mPos].ch, fields[rec->assign[mPos].fieldId]);
                    snprintf(mVal, 40, "%g", ah::round3(mIv->getValue(mPos, rec)));
                    mPublish(mSubTopic, mVal, retained, qos);
                }
                mPos++;
            } else {
                if (MqttSentStatus::LAST_SUCCESS_SENT == rec->mqttSentStatus) {
                    sendRadioStat(rec->length);
                    rec->mqttSentStatus = MqttSentStatus::DATA_SENT;
                }
                mState = FIND_NXT_IV;
            }
        }

        inline void sendRadioStat(uint8_t start) {
            snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/radio_stat", mIv->config->name);
            snprintf(mVal, 140, "{\"tx\":%d,\"success\":%d,\"fail\":%d,\"no_answer\":%d,\"retransmits\":%d,\"lossIvRx\":%d,\"lossIvTx\":%d,\"lossDtuRx\":%d,\"lossDtuTx\":%d}",
                mIv->radioStatistics.txCnt,
                mIv->radioStatistics.rxSuccess,
                mIv->radioStatistics.rxFail,
                mIv->radioStatistics.rxFailNoAnser,
                mIv->radioStatistics.retransmits,
                mIv->radioStatistics.ivLoss,
                mIv->radioStatistics.ivSent,
                mIv->radioStatistics.dtuLoss,
                mIv->radioStatistics.dtuSent);
            mPublish(mSubTopic, mVal, false, QOS_0);
        }

        void stateSendTotals() {
            uint8_t fieldId;
            mRTRDataHasBeenSent = true;
            if(mPos < 4) {
                bool retained = true;
                switch (mPos) {
                    default:
                    case 0:
                        fieldId = FLD_PAC;
                        retained = false;
                        break;
                    case 1:
                        if(!mAllTotalFound) {
                            mPos++;
                            return;
                        }
                        fieldId = FLD_YT;
                        break;
                    case 2:
                        if((!mAllTotalFound) || (!mSendTotalYd)) {
                            mPos++;
                            return;
                        }
                        fieldId = FLD_YD;
                        break;
                    case 3:
                        fieldId = FLD_PDC;
                        retained = false;
                        break;
                }
                snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "total/%s", fields[fieldId]);
                snprintf(mVal, 40, "%g", ah::round3(mTotal[mPos]));
                mPublish(mSubTopic, mVal, retained, QOS_0);
                mPos++;
            } else {
                mSendList->pop();
                mZeroValues = false;
                mPos = 0;
                mState = IDLE;
            }
        }

        HMSYSTEM *mSys;
        uint32_t *mUtcTimestamp;
        pubMqttPublisherType mPublish;
        State mState;
        StateFunction mTable[NUM_STATES];

        uint8_t mCmd;
        uint8_t mLastIvId;
        bool mSendTotals, mTotalFound, mAllTotalFound, mSendTotalYd;
        float mTotal[4];

        Inverter<> *mIv, *mIvSend;
        uint8_t mPos;
        bool mRTRDataHasBeenSent;

        char mSubTopic[32 + MAX_NAME_LENGTH + 1];
        char mVal[140];
        bool mZeroValues; // makes sure that yield day is sent even if no inverter is online

        std::queue<sendListCmdIv> *mSendList;
};

#endif /*__PUB_MQTT_IV_DATA_H__*/
