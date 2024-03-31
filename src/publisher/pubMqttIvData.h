    //-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __PUB_MQTT_IV_DATA_H__
#define __PUB_MQTT_IV_DATA_H__

#include <array>
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
        PubMqttIvData() : mTotal{}, mSubTopic{}, mVal{} {}

        void setup(HMSYSTEM *sys, bool json, uint32_t *utcTs, std::queue<sendListCmdIv> *sendList) {
            mSys           = sys;
            mJson          = json;
            mUtcTimestamp  = utcTs;
            mSendList      = sendList;
            mState         = IDLE;
            mYldTotalStore = 0;

            mRTRDataHasBeenSent = false;

            mTable[IDLE]        = &PubMqttIvData::stateIdle;
            mTable[START]       = &PubMqttIvData::stateStart;
            mTable[FIND_NXT_IV] = &PubMqttIvData::stateFindNxtIv;
            mTable[SEND_DATA]   = &PubMqttIvData::stateSend;
            mTable[SEND_TOTALS] = &PubMqttIvData::stateSendTotals;
        }

        void loop() {
            (this->*mTable[mState])();
            yield();
        }

        void resetYieldDay() {
            mYldTotalStore = 0;
        }

        bool start() {
            if(IDLE != mState)
                return false;

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
            mAtLeastOneWasntSent = false;
            if(!mSendList->empty()) {
                mCmd = mSendList->front().cmd;
                mIvSend = mSendList->front().iv;

                if((RealTimeRunData_Debug != mCmd) || !mRTRDataHasBeenSent) { // send RealTimeRunData only once
                    mSendTotals = (RealTimeRunData_Debug == mCmd);
                    memset(mTotal, 0, sizeof(float) * 5);
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
                    snprintf(mSubTopic.data(), mSubTopic.size(), "%s/last_success", mIv->config->name);
                    snprintf(mVal.data(), mVal.size(), "%d", mIv->getLastTs(rec));
                    mPublish(mSubTopic.data(), mVal.data(), true, QOS_0);

                    if((mIv->ivGen == IV_HMS) || (mIv->ivGen == IV_HMT)) {
                        snprintf(mSubTopic.data(), mSubTopic.size(), "%s/rssi", mIv->config->name);
                        snprintf(mVal.data(), mVal.size(), "%d", mIv->rssi);
                        mPublish(mSubTopic.data(), mVal.data(), false, QOS_0);
                    }
                    rec->mqttSentStatus = MqttSentStatus::LAST_SUCCESS_SENT;
                }

                mIv->isProducing(); // recalculate status
                mState = SEND_DATA;
            } else if(mSendTotals && mTotalFound && mAtLeastOneWasntSent) {
                if(mYldTotalStore > mTotal[2])
                    mSendTotalYd = false; // don't send yield total if last value was greater
                else
                    mYldTotalStore = mTotal[2];

                mState = SEND_TOTALS;
            } else {
                mSendList->pop();
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
                if (RealTimeRunData_Debug == mCmd) {
                    if((FLD_YT == rec->assign[mPos].fieldId) || (FLD_YD == rec->assign[mPos].fieldId))
                        retained = true;

                    // calculate total values for RealTimeRunData_Debug
                    if (CH0 == rec->assign[mPos].ch) {
                        if(mIv->getStatus() != InverterStatus::OFF) {
                            mTotalFound = true;
                            switch (rec->assign[mPos].fieldId) {
                                case FLD_PAC:
                                    mTotal[0] += mIv->getValue(mPos, rec);
                                    break;
                                case FLD_YT:
                                    mTotal[1] += mIv->getValue(mPos, rec);
                                    break;
                                case FLD_YD: {
                                    mTotal[2] += mIv->getValue(mPos, rec);
                                    break;
                                }
                                case FLD_PDC:
                                    mTotal[3] += mIv->getValue(mPos, rec);
                                    break;
                                case FLD_MP:
                                    mTotal[4] += mIv->getValue(mPos, rec);
                                    break;
                            }
                        } else
                            mAllTotalFound = false;
                    }
                }

                if (MqttSentStatus::LAST_SUCCESS_SENT == rec->mqttSentStatus) {
                    mAtLeastOneWasntSent = true;
                    if(InverterDevInform_All == mCmd) {
                        snprintf(mSubTopic.data(), mSubTopic.size(), "%s/firmware", mIv->config->name);
                        snprintf(mVal.data(), mVal.size(), "{\"version\":%d,\"build_year\":\"%d\",\"build_month_day\":%d,\"build_hour_min\":%d,\"bootloader\":%d}",
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_FW_VERSION, rec)),
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_FW_BUILD_YEAR, rec)),
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_FW_BUILD_MONTH_DAY, rec)),
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_FW_BUILD_HOUR_MINUTE, rec)),
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_BOOTLOADER_VER, rec)));
                        retained = true;
                    } else if(InverterDevInform_Simple == mCmd) {
                        snprintf(mSubTopic.data(), mSubTopic.size(), "%s/hardware", mIv->config->name);
                        snprintf(mVal.data(), mVal.size(), "{\"part\":%d,\"version\":\"%d\",\"grid_profile_code\":%d,\"grid_profile_version\":%d}",
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_PART_NUM, rec)),
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_HW_VERSION, rec)),
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_GRID_PROFILE_CODE, rec)),
                            static_cast<int>(mIv->getChannelFieldValue(CH0, FLD_GRID_PROFILE_VERSION, rec)));
                        retained = true;
                    } else {
                        if (!mJson) {
                            snprintf(mSubTopic.data(), mSubTopic.size(), "%s/ch%d/%s", mIv->config->name, rec->assign[mPos].ch, fields[rec->assign[mPos].fieldId]);
                            snprintf(mVal.data(), mVal.size(), "%g", ah::round3(mIv->getValue(mPos, rec)));
                        }
                    }

                    if (InverterDevInform_All == mCmd || InverterDevInform_Simple == mCmd || !mJson) {
                        uint8_t qos = (FLD_ACT_ACTIVE_PWR_LIMIT == rec->assign[mPos].fieldId) ? QOS_2 : QOS_0;
                        if((FLD_EVT != rec->assign[mPos].fieldId)
                            && (FLD_LAST_ALARM_CODE != rec->assign[mPos].fieldId))
                            mPublish(mSubTopic.data(), mVal.data(), retained, qos);
                    }
                }
                mPos++;
            } else {
                if (MqttSentStatus::LAST_SUCCESS_SENT == rec->mqttSentStatus) {
                    if (mJson) {
                        DynamicJsonDocument doc(300);
                        std::array<char, 300> buf;
                        int ch = rec->assign[0].ch;

                        for (mPos = 0; mPos <= rec->length; mPos++) {
                            if (rec->assign[mPos].ch != ch) {
                                // if next channel.. publish
                                serializeJson(doc, buf.data(), buf.size());
                                doc.clear();
                                snprintf(mSubTopic.data(), mSubTopic.size(), "%s/ch%d", mIv->config->name, ch);
                                mPublish(mSubTopic.data(), buf.data(), false, QOS_0);
                                ch = rec->assign[mPos].ch;
                            }
                            if (mPos == rec->length)
                                break;

                            doc[fields[rec->assign[mPos].fieldId]] = ah::round3(mIv->getValue(mPos, rec));
                        }
                    }

                    sendRadioStat(rec->length);
                    rec->mqttSentStatus = MqttSentStatus::DATA_SENT;
                }
                mState = FIND_NXT_IV;
            }
        }

        inline void sendRadioStat(uint8_t start) {
            snprintf(mSubTopic.data(), mSubTopic.size(), "%s/radio_stat", mIv->config->name);
            snprintf(mVal.data(), mVal.size(), "{\"tx\":%d,\"success\":%d,\"fail\":%d,\"no_answer\":%d,\"retransmits\":%d,\"lossIvRx\":%d,\"lossIvTx\":%d,\"lossDtuRx\":%d,\"lossDtuTx\":%d}",
                mIv->radioStatistics.txCnt,
                mIv->radioStatistics.rxSuccess,
                mIv->radioStatistics.rxFail,
                mIv->radioStatistics.rxFailNoAnswer,
                mIv->radioStatistics.retransmits,
                mIv->radioStatistics.ivLoss,
                mIv->radioStatistics.ivSent,
                mIv->radioStatistics.dtuLoss,
                mIv->radioStatistics.dtuSent);
            mPublish(mSubTopic.data(), mVal.data(), false, QOS_0);
        }

        void stateSendTotals() {
            mRTRDataHasBeenSent = true;
            if(mPos < 5) {
                uint8_t fieldId;
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
                    case 4:
                        fieldId = FLD_MP;
                        retained = false;
                        break;
                }
                if (!mJson) {
                    snprintf(mSubTopic.data(), mSubTopic.size(), "total/%s", fields[fieldId]);
                    snprintf(mVal.data(), mVal.size(), "%g", ah::round3(mTotal[mPos]));
                    mPublish(mSubTopic.data(), mVal.data(), retained, QOS_0);
                }
                mPos++;
            } else {
                if (mJson) {
                    int type[5] = {FLD_PAC, FLD_YT, FLD_YD, FLD_PDC, FLD_MP};
                    snprintf(mVal.data(), mVal.size(), "{");

                    for (mPos = 0; mPos < 5; mPos++) {
                        snprintf(mSubTopic.data(), mSubTopic.size(), "\"%s\":%g", fields[type[mPos]], ah::round3(mTotal[mPos]));
                        strcat(mVal.data(), mSubTopic.data());
                        if (mPos < 4)
                            strcat(mVal.data(), ",");
                        else
                            strcat(mVal.data(), "}");
                    }
                    mPublish(F("total"), mVal.data(), true, QOS_0);
                }
                mSendList->pop();
                mSendTotals = false;
                mState = IDLE;
            }
        }

        HMSYSTEM *mSys = nullptr;
        uint32_t *mUtcTimestamp = nullptr;
        pubMqttPublisherType mPublish;
        State mState = IDLE;
        StateFunction mTable[NUM_STATES];

        uint8_t mCmd = 0;
        uint8_t mLastIvId = 0;
        bool mSendTotals = false, mTotalFound = false, mAllTotalFound = false;
        bool mSendTotalYd = false, mAtLeastOneWasntSent = false;
        float mTotal[5], mYldTotalStore = 0;

        Inverter<> *mIv = nullptr, *mIvSend = nullptr;
        uint8_t mPos = 0;
        bool mRTRDataHasBeenSent = false;

        std::array<char, (32 + MAX_NAME_LENGTH + 1)> mSubTopic;
        std::array<char, 160> mVal;
        bool mJson;

        std::queue<sendListCmdIv> *mSendList = nullptr;
};

#endif /*__PUB_MQTT_IV_DATA_H__*/
