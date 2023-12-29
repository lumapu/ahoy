//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#include "CommQueue.h"
#include <Arduino.h>
#include "../utils/crc.h"
#include "../utils/timemonitor.h"
#include "Heuristic.h"

#define MI_TIMEOUT          250 // timeout for MI type requests
#define FRSTMSG_TIMEOUT     150 // how long to wait for first msg to be received
#define DEFAULT_TIMEOUT     500 // timeout for regular requests
#define SINGLEFR_TIMEOUT    100 // timeout for single frame requests
#define MAX_BUFFER          250

typedef std::function<void(uint8_t, Inverter<> *)> payloadListenerType;
typedef std::function<void(Inverter<> *)> alarmListenerType;

class Communication : public CommQueue<> {
    public:
        void setup(uint32_t *timestamp, bool *serialDebug, bool *privacyMode, bool *printWholeTrace, uint16_t *inverterGap) {
            mTimestamp = timestamp;
            mPrivacyMode = privacyMode;
            mSerialDebug = serialDebug;
            mPrintWholeTrace = printWholeTrace;
            mInverterGap = inverterGap;
        }

        void addImportant(Inverter<> *iv, uint8_t cmd) {
            mState = States::RESET; // cancel current operation
            CommQueue::addImportant(iv, cmd);
        }

        void addPayloadListener(payloadListenerType cb) {
            mCbPayload = cb;
        }

        void addAlarmListener(alarmListenerType cb) {
            mCbAlarm = cb;
        }

        void loop() {
            get([this](bool valid, const queue_s *q) {
                if(!valid) {
                    if(mPrintSequenceDuration) {
                        mPrintSequenceDuration = false;
                        DPRINT(DBG_INFO, F("com loop duration: "));
                        DBGPRINT(String(millis() - mLastEmptyQueueMillis));
                        DBGPRINTLN(F("ms"));
                        DBGPRINTLN(F("-----"));
                    }
                    return; // empty
                }
                if(!mPrintSequenceDuration) // entry was added to the queue
                    mLastEmptyQueueMillis = millis();
                mPrintSequenceDuration = true;

                uint16_t timeout     = (q->iv->ivGen == IV_MI) ? MI_TIMEOUT : (((q->iv->mGotFragment && q->iv->mGotLastMsg) || mIsRetransmit) ? SINGLEFR_TIMEOUT : ((q->cmd != AlarmData) && (q->cmd != GridOnProFilePara) ? DEFAULT_TIMEOUT : (1.5 * DEFAULT_TIMEOUT)));

                /*if(mDebugState != mState) {
                    DPRINT(DBG_INFO, F("State: "));
                    DBGHEXLN((uint8_t)(mState));
                    mDebugState = mState;
                }*/
                switch(mState) {
                    case States::RESET:
                        if (!mWaitTime.isTimeout())
                            return;

                        mMaxFrameId = 0;
                        for(uint8_t i = 0; i < MAX_PAYLOAD_ENTRIES; i++) {
                            mLocalBuf[i].len = 0;
                        }

                        if(*mSerialDebug)
                            mHeu.printStatus(q->iv);
                        mHeu.getTxCh(q->iv);
                        q->iv->mGotFragment = false;
                        q->iv->mGotLastMsg  = false;
                        q->iv->curFrmCnt    = 0;
                        mIsRetransmit = false;
                        if(NULL == q->iv->radio)
                            cmdDone(false); // can't communicate while radio is not defined!
                        q->iv->mCmd = q->cmd;
                        q->iv->mIsSingleframeReq = false;
                        mState = States::START;
                        break;

                    case States::START:
                        setTs(mTimestamp);
                        if((IV_HMS == q->iv->ivGen) || (IV_HMT == q->iv->ivGen)) {
                            // frequency was changed during runtime
                            if(q->iv->curCmtFreq != q->iv->config->frequency) {
                                if(q->iv->radio->switchFrequencyCh(q->iv, q->iv->curCmtFreq, q->iv->config->frequency))
                                    q->iv->curCmtFreq = q->iv->config->frequency;
                            }
                        }

                        if(q->isDevControl) {
                            if(ActivePowerContr == q->cmd)
                                q->iv->powerLimitAck = false;
                            q->iv->radio->sendControlPacket(q->iv, q->cmd, q->iv->powerLimit, false);
                        } else
                            q->iv->radio->prepareDevInformCmd(q->iv, q->cmd, q->ts, q->iv->alarmLastId, false);

                        q->iv->radioStatistics.txCnt++;
                        mWaitTime.startTimeMonitor(timeout);
                        mIsRetransmit    = false;
                        setAttempt();
                        if((q->cmd == AlarmData) || (q->cmd == GridOnProFilePara))
                            incrAttempt(q->cmd == AlarmData? 5 : 3);

                        mState = States::WAIT;
                        break;

                    case States::WAIT:
                        if (!mWaitTime.isTimeout())
                            return;
                        mState = States::CHECK_FRAMES;
                        break;

                    case States::CHECK_FRAMES: {
                        if((q->iv->radio->mBufCtrl.empty() && !mIsRetransmit) || (0 == q->attempts)) { // radio buffer empty or no more answers
                            if(*mSerialDebug) {
                                DPRINT_IVID(DBG_INFO, q->iv->id);
                                DBGPRINT(F("request timeout: "));
                                DBGPRINT(String(mWaitTime.getRunTime()));
                                DBGPRINTLN(F("ms"));
                            }
                            if(!q->iv->mGotFragment) {
                                if((IV_HMS == q->iv->ivGen) || (IV_HMT == q->iv->ivGen)) {
                                    q->iv->radio->switchFrequency(q->iv, HOY_BOOT_FREQ_KHZ, (q->iv->config->frequency*FREQ_STEP_KHZ + HOY_BASE_FREQ_KHZ));
                                    mWaitTime.startTimeMonitor(1000);
                                }
                            }
                            closeRequest(q, false);
                            break;
                        }
                        mFirstTry = false; // for correct reset
                        if((IV_MI != q->iv->ivGen) || (0 == q->attempts))
                            mIsRetransmit = false;

                        while(!q->iv->radio->mBufCtrl.empty()) {
                            packet_t *p = &q->iv->radio->mBufCtrl.front();
                            printRxInfo(q, p);

                            if(validateIvSerial(&p->packet[1], q->iv)) {
                                q->iv->radioStatistics.frmCnt++;
                                q->iv->mDtuRxCnt++;

                                if (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES)) {  // response from get information command
                                    if(parseFrame(p))
                                        q->iv->curFrmCnt++;
                                } else if (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES)) { // response from dev control command
                                    if(parseDevCtrl(p, q))
                                        closeRequest(q, true);
                                    else
                                        closeRequest(q, false);
                                    q->iv->radio->mBufCtrl.pop();
                                    return; // don't wait for empty buffer
                                } else if(IV_MI == q->iv->ivGen) {
                                    if(parseMiFrame(p, q))
                                        q->iv->curFrmCnt++;
                                }
                            } //else -> serial does not match

                            q->iv->radio->mBufCtrl.pop();
                            yield();
                        }

                        if(0 == q->attempts) {
                            DPRINT_IVID(DBG_INFO, q->iv->id);
                            DBGPRINT(F("no attempts left"));
                            closeRequest(q, false);
                        } else {
                            if(q->iv->ivGen != IV_MI) {
                                mState = States::CHECK_PACKAGE;
                            } else {
                                bool fastNext = true;
                                if(q->iv->miMultiParts < 6) {
                                    mState = States::WAIT;
                                    if((mWaitTime.isTimeout() && mIsRetransmit) || !mIsRetransmit) {
                                        miRepeatRequest(q);
                                        return;
                                    }
                                } else {
                                    mHeu.evalTxChQuality(q->iv, true, (4 - q->attempts), q->iv->curFrmCnt);
                                    if(((q->cmd == 0x39) && (q->iv->type == INV_TYPE_4CH))
                                        || ((q->cmd == MI_REQ_CH2) && (q->iv->type == INV_TYPE_2CH))
                                        || ((q->cmd == MI_REQ_CH1) && (q->iv->type == INV_TYPE_1CH))) {
                                        miComplete(q->iv);
                                        fastNext = false;
                                    }
                                    if(fastNext)
                                        miNextRequest(q->iv->type == INV_TYPE_4CH ? MI_REQ_4CH : MI_REQ_CH1, q);
                                    else
                                        closeRequest(q, true);
                                }
                            }
                        }

                        }
                        break;

                    case States::CHECK_PACKAGE:
                        uint8_t framnr = 0;
                        if(0 == mMaxFrameId) {
                            uint8_t i = 0;
                            while(i < MAX_PAYLOAD_ENTRIES) {
                                if(mLocalBuf[i].len == 0) {
                                    framnr = i+1;
                                    break;
                                }
                                i++;
                            }
                        }

                        if(!framnr) {
                            for(uint8_t i = 0; i < mMaxFrameId; i++) {
                                if(mLocalBuf[i].len == 0) {
                                    framnr = i+1;
                                    break;
                                }
                            }
                        }

                        if(framnr) {
                            setAttempt();

                            if(*mSerialDebug) {
                                DPRINT_IVID(DBG_WARN, q->iv->id);
                                DBGPRINT(F("frame "));
                                DBGPRINT(String(framnr));
                                DBGPRINT(F(" missing: request retransmit ("));
                                DBGPRINT(String(q->attempts));
                                DBGPRINTLN(F(" attempts left)"));
                            }
                            if (!mIsRetransmit)
                                q->iv->mIsSingleframeReq = true;
                            sendRetransmit(q, (framnr-1));
                            mIsRetransmit = true;
                            return;
                        }

                        compilePayload(q);

                        if((NULL != mCbPayload) && (GridOnProFilePara != q->cmd) && (GetLossRate != q->cmd))
                            (mCbPayload)(q->cmd, q->iv);

                        closeRequest(q, true);
                        break;
                }
            });
        }

    private:
        inline void printRxInfo(const queue_s *q, packet_t *p) {
            DPRINT_IVID(DBG_INFO, q->iv->id);
            DBGPRINT(F("RX "));
            if(p->millis < 100)
                DBGPRINT(F(" "));
            DBGPRINT(String(p->millis));
            DBGPRINT(F("ms | "));
            DBGPRINT(String(p->len));
            if((IV_HM == q->iv->ivGen) || (IV_MI == q->iv->ivGen)) {
                DBGPRINT(F(" CH"));
                if(3 == p->ch)
                    DBGPRINT(F("0"));
                DBGPRINT(String(p->ch));
                DBGPRINT(F(" "));
            } else {
                DBGPRINT(F(" "));
                DBGPRINT(String(p->rssi));
                DBGPRINT(F("dBm | "));
            }
            if(*mPrintWholeTrace) {
                if(*mPrivacyMode)
                    ah::dumpBuf(p->packet, p->len, 1, 8);
                else
                    ah::dumpBuf(p->packet, p->len);
            } else {
                DBGPRINT(F("| "));
                DHEX(p->packet[0]);
                DBGPRINT(F(" "));
                DBGHEXLN(p->packet[9]);
            }
        }

        inline bool validateIvSerial(uint8_t buf[], Inverter<> *iv) {
            uint8_t tmp[4];
            CP_U32_BigEndian(tmp, iv->radioId.u64 >> 8);
            for(uint8_t i = 0; i < 4; i++) {
                if(tmp[i] != buf[i]) {
                    DPRINT(DBG_WARN, F("Inverter serial does not match, got: 0x"));
                    DHEX(buf[0]);DHEX(buf[1]);DHEX(buf[2]);DHEX(buf[3]);
                    DBGPRINT(F(", expected: 0x"));
                    DHEX(tmp[0]);DHEX(tmp[1]);DHEX(tmp[2]);DHEX(tmp[3]);
                    DBGPRINTLN("");
                    return false;
                }
            }
            return true;
        }

        inline bool checkFrameCrc(uint8_t buf[], uint8_t len) {
            return (ah::crc8(buf, len - 1) == buf[len-1]);
        }

        inline bool parseFrame(packet_t *p) {
            uint8_t *frameId = &p->packet[9];
            if(0x00 == *frameId) {
                DPRINTLN(DBG_WARN, F("invalid frameId 0x00"));
                return false; // skip current packet
            }
            if((*frameId & 0x7f) > MAX_PAYLOAD_ENTRIES) {
                DPRINTLN(DBG_WARN, F("local buffer to small for payload fragments"));
                return false; // local storage is to small for id
            }

            if(!checkFrameCrc(p->packet, p->len)) {
                DPRINTLN(DBG_WARN, F("frame CRC is wrong"));
                return false; // CRC8 is wrong, frame invalid
            }

            if((*frameId & ALL_FRAMES) == ALL_FRAMES) {
                mMaxFrameId = (*frameId & 0x7f);
                if(mMaxFrameId > 8) // large payloads, e.g. AlarmData
                    incrAttempt(mMaxFrameId - 6);
            }

            frame_t *f = &mLocalBuf[(*frameId & 0x7f) - 1];
            memcpy(f->buf, &p->packet[10], p->len-11);
            f->len  = p->len - 11;
            f->rssi = p->rssi;

            return true;
        }

        inline bool parseMiFrame(packet_t *p, const queue_s *q) {
            if ((p->packet[0] == MI_REQ_CH1 + ALL_FRAMES)
                || (p->packet[0] == MI_REQ_CH2 + ALL_FRAMES)
                || ((p->packet[0] >= (MI_REQ_4CH + ALL_FRAMES))
                    && (p->packet[0] < (0x39 + SINGLE_FRAME))
                    )) {    //&& (p->packet[0] != (0x0f + ALL_FRAMES)))) {
                // small MI or MI 1500 data responses to 0x09, 0x11, 0x36, 0x37, 0x38 and 0x39
                //mPayload[iv->id].txId = p->packet[0];
                miDataDecode(p, q);
            } else if (p->packet[0] == (0x0f + ALL_FRAMES))
                miHwDecode(p, q);
            else if ((p->packet[0] == 0x88) || (p->packet[0] == 0x92)) {
                record_t<> *rec = q->iv->getRecordStruct(RealTimeRunData_Debug);  // choose the record structure
                rec->ts = q->ts;
                miStsConsolidate(q, ((p->packet[0] == 0x88) ? 1 : 2), rec, p->packet[10], p->packet[12], p->packet[9], p->packet[11]);
                //mHeu.setGotFragment(q->iv); only do this when we are through the cycle?
            }

            return true;
        }

        inline bool parseDevCtrl(packet_t *p, const queue_s *q) {
            if((p->packet[12] != ActivePowerContr) || (p->packet[13] != 0x00))
                return false;
            bool accepted = true;
            if((p->packet[10] == 0x00) && (p->packet[11] == 0x00))
                q->iv->powerLimitAck = true;
            else
                accepted = false;

            DPRINT_IVID(DBG_INFO, q->iv->id);
            DBGPRINT(F("has "));
            if(!accepted) DBGPRINT(F("not "));
            DBGPRINT(F("accepted power limit set point "));
            DBGPRINT(String(q->iv->powerLimit[0]));
            DBGPRINT(F(" with PowerLimitControl "));
            DBGPRINTLN(String(q->iv->powerLimit[1]));
            q->iv->actPowerLimit = 0xffff; // unknown, readback current value

            return accepted;
        }

        inline void compilePayload(const queue_s *q) {
            uint16_t crc = 0xffff, crcRcv = 0x0000;
            for(uint8_t i = 0; i < mMaxFrameId; i++) {
                if(i == (mMaxFrameId - 1)) {
                    crc = ah::crc16(mLocalBuf[i].buf, mLocalBuf[i].len - 2, crc);
                    crcRcv = (mLocalBuf[i].buf[mLocalBuf[i].len-2] << 8);
                    crcRcv |= mLocalBuf[i].buf[mLocalBuf[i].len-1];
                } else
                    crc = ah::crc16(mLocalBuf[i].buf, mLocalBuf[i].len, crc);
            }

            if(crc != crcRcv) {
                DPRINT_IVID(DBG_WARN, q->iv->id);
                DBGPRINT(F("CRC Error "));
                if(q->attempts == 0) {
                    DBGPRINTLN(F("-> Fail"));
                    closeRequest(q, false);

                } else
                    DBGPRINTLN(F("-> complete retransmit"));
                mState = States::RESET;
                return;
            }

            /*DPRINT_IVID(DBG_INFO, q->iv->id);
            DBGPRINT(F("procPyld: cmd:  0x"));
            DBGHEXLN(q->cmd);*/

            memset(mPayload, 0, MAX_BUFFER);
            int8_t rssi = -127;
            uint8_t len = 0;

            for(uint8_t i = 0; i < mMaxFrameId; i++) {
                if(mLocalBuf[i].len + len > MAX_BUFFER) {
                    DPRINTLN(DBG_ERROR, F("payload buffer to small!"));
                    return;
                }
                memcpy(&mPayload[len], mLocalBuf[i].buf, mLocalBuf[i].len);
                len += mLocalBuf[i].len;
                // get worst RSSI (high value is better)
                if(mLocalBuf[i].rssi > rssi)
                    rssi = mLocalBuf[i].rssi;
            }

            len -= 2;

            DPRINT_IVID(DBG_INFO, q->iv->id);
            DBGPRINT(F("Payload ("));
            DBGPRINT(String(len));
            if(*mPrintWholeTrace) {
                DBGPRINT(F("): "));
                ah::dumpBuf(mPayload, len);
            } else
                DBGPRINTLN(F(")"));

            if(GridOnProFilePara == q->cmd) {
                q->iv->addGridProfile(mPayload, len);
                return;
            }

            record_t<> *rec = q->iv->getRecordStruct(q->cmd);
            if(NULL == rec) {
                if(GetLossRate == q->cmd) {
                    q->iv->parseGetLossRate(mPayload, len);
                    return;
                } else {
                    DPRINTLN(DBG_ERROR, F("record is NULL!"));
                    closeRequest(q, false);
                }
                return;
            }
            if((rec->pyldLen != len) && (0 != rec->pyldLen)) {
                if(*mSerialDebug) {
                    DPRINT(DBG_ERROR, F("plausibility check failed, expected "));
                    DBGPRINT(String(rec->pyldLen));
                    DBGPRINTLN(F(" bytes"));
                }
                /*q->iv->radioStatistics.rxFail++;*/
                closeRequest(q, false);

                return;
            }

            rec->ts = q->ts;
            for (uint8_t i = 0; i < rec->length; i++) {
                q->iv->addValue(i, mPayload, rec);
            }

            q->iv->rssi = rssi;
            q->iv->doCalculations();

            if(AlarmData == q->cmd) {
                uint8_t i = 0;
                while(1) {
                    if(0 == q->iv->parseAlarmLog(i++, mPayload, len))
                        break;
                    if (NULL != mCbAlarm)
                        (mCbAlarm)(q->iv);
                    yield();
                }
            }
        }

        void sendRetransmit(const queue_s *q, uint8_t i) {
            if(q->attempts) {
                q->iv->radio->sendCmdPacket(q->iv, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                q->iv->radioStatistics.retransmits++;
                mWaitTime.startTimeMonitor(SINGLEFR_TIMEOUT); // timeout
                mState = States::WAIT;
            } else {
                //add(q, true);
                closeRequest(q, false);
            }
        }

    private:
        void closeRequest(const queue_s *q, bool crcPass) {
            mHeu.evalTxChQuality(q->iv, crcPass, (4 - q->attempts), q->iv->curFrmCnt);
            if(crcPass)
                q->iv->radioStatistics.rxSuccess++;
            else if(q->iv->mGotFragment)
                q->iv->radioStatistics.rxFail++; // got no complete payload
            else
                q->iv->radioStatistics.rxFailNoAnser++; // got nothing
            mWaitTime.startTimeMonitor(*mInverterGap);

            bool keep = false;
            if(q->isDevControl)
                keep = !crcPass;

            cmdDone(keep);
            q->iv->mGotFragment = false;
            q->iv->mGotLastMsg  = false;
            q->iv->miMultiParts = 0;
            mIsRetransmit       = false;
            mFirstTry           = false; // for correct reset
            mState              = States::RESET;
            DBGPRINTLN(F("-----"));
        }

        inline void miHwDecode(packet_t *p, const queue_s *q) {
            record_t<> *rec = q->iv->getRecordStruct(InverterDevInform_All);  // choose the record structure
            rec->ts = q->ts;
            /*
            Polling the device software and hardware version number command
            start byte  Command word     routing address                 target address              User data   check   end byte
            byte[0]  byte[1]     byte[2]     byte[3]     byte[4]     byte[5]     byte[6]     byte[7]     byte[8]     byte[9]     byte[10]    byte[11]    byte[12]
            0x7e     0x0f    xx  xx  xx  xx  YY  YY  YY  YY  0x00    CRC     0x7f
            Command Receipt - First Frame
            start byte  Command word     target address              routing address                 Multi-frame marking     User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   check   end byte
            byte[0]  byte[1]     byte[2]     byte[3]     byte[4]     byte[5]     byte[6]     byte[7]     byte[8]     byte[9]     byte[10]    byte[11]    byte[12]    byte[13]    byte[14]    byte[15]    byte[16]    byte[17]    byte[18]    byte[19]    byte[20]    byte[21]    byte[22]    byte[23]    byte[24]    byte[25]    byte[26]    byte[27]    byte[28]
            0x7e     0x8f    YY  YY  YY  YY  xx  xx  xx  xx  0x00    USFWBuild_VER       APPFWBuild_VER      APPFWBuild_YYYY         APPFWBuild_MMDD         APPFWBuild_HHMM         APPFW_PN                HW_VER      CRC     0x7f
            Command Receipt - Second Frame
            start byte  Command word     target address              routing address                 Multi-frame marking     User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   User data   check   end byte
            byte[0]  byte[1]     byte[2]     byte[3]     byte[4]     byte[5]     byte[6]     byte[7]     byte[8]     byte[9]     byte[10]    byte[11]    byte[12]    byte[13]    byte[14]    byte[15]    byte[16]    byte[17]    byte[18]    byte[19]    byte[20]    byte[21]    byte[22]    byte[23]    byte[24]    byte[25]    byte[26]    byte[27]    byte[28]
            0x7e     0x8f    YY  YY  YY  YY  xx  xx  xx  xx  0x01    HW_PN               HW_FB_TLmValue      HW_FB_ReSPRT        HW_GridSamp_ResValule       HW_ECapValue        Matching_APPFW_PN               CRC     0x7f
            Command receipt - third frame
            start byte  Command word     target address              routing address                 Multi-frame marking     User data   User data   User data   User data   User data   User data   User data   User data   check   end byte
            byte[0]  byte[1]     byte[2]     byte[3]     byte[4]     byte[5]     byte[6]     byte[7]     byte[8]     byte[9]     byte[10]    byte[11]    byte[12]    byte[13]    byte[14]    byte[15]    byte[16]    byte[15]    byte[16]    byte[17]    byte[18]
            0x7e     0x8f    YY  YY  YY  YY  xx  xx  xx  xx  0x12    APPFW_MINVER        HWInfoAddr      PNInfoCRC_gusv      PNInfoCRC_gusv      CRC     0x7f
            */

            /*
            case InverterDevInform_All:
                        rec->length  = (uint8_t)(HMINFO_LIST_LEN);
                        rec->assign  = (byteAssign_t *)InfoAssignment;
                        rec->pyldLen = HMINFO_PAYLOAD_LEN;
                        break;
            const byteAssign_t InfoAssignment[] = {
            { FLD_FW_VERSION,           UNIT_NONE,   CH0,  0, 2, 1 },
            { FLD_FW_BUILD_YEAR,        UNIT_NONE,   CH0,  2, 2, 1 },
            { FLD_FW_BUILD_MONTH_DAY,   UNIT_NONE,   CH0,  4, 2, 1 },
            { FLD_FW_BUILD_HOUR_MINUTE, UNIT_NONE,   CH0,  6, 2, 1 },
            { FLD_BOOTLOADER_VER,       UNIT_NONE,   CH0,  8, 2, 1 }
            };
            */

            if ( p->packet[9] == 0x00 ) {//first frame
                //FLD_FW_VERSION
                for (uint8_t i = 0; i < 5; i++) {
                    q->iv->setValue(i, rec, (float) ((p->packet[(12+2*i)] << 8) + p->packet[(13+2*i)])/1);
                }
                q->iv->isConnected = true;
                if(*mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, q->iv->id);
                    DBGPRINT(F("HW_VER is "));
                    DBGPRINTLN(String((p->packet[24] << 8) + p->packet[25]));
                }
                record_t<> *rec = q->iv->getRecordStruct(InverterDevInform_Simple);  // choose the record structure
                rec->ts = q->ts;
                q->iv->setValue(1, rec, (uint32_t) ((p->packet[24] << 8) + p->packet[25])/1);
                q->iv->miMultiParts +=4;
            } else if ( p->packet[9] == 0x01 || p->packet[9] == 0x10 ) {//second frame for MI, 3rd gen. answers in 0x10
                DPRINT_IVID(DBG_INFO, q->iv->id);
                if ( p->packet[9] == 0x01 ) {
                    DBGPRINTLN(F("got 2nd frame (hw info)"));
                    /* according to xlsx (different start byte -1!)
                    byte[11] to	 byte[14] HW_PN
                    byte[15]	 byte[16] HW_FB_TLmValue
                    byte[17]	 byte[18] HW_FB_ReSPRT
                    byte[19]	 byte[20] HW_GridSamp_ResValule
                    byte[21]	 byte[22] HW_ECapValue
                    byte[23] to	 byte[26] Matching_APPFW_PN*/
                    DPRINT(DBG_INFO,F("HW_PartNo "));
                    DBGPRINTLN(String((uint32_t) (((p->packet[10] << 8) | p->packet[11]) << 8 | p->packet[12]) << 8 | p->packet[13]));
                    record_t<> *rec = q->iv->getRecordStruct(InverterDevInform_Simple);  // choose the record structure
                    rec->ts = q->ts;
                    q->iv->setValue(0, rec, (uint32_t) ((((p->packet[10] << 8) | p->packet[11]) << 8 | p->packet[12]) << 8 | p->packet[13])/1);

                    if(*mSerialDebug) {
                        DPRINT(DBG_INFO,F("HW_FB_TLmValue "));
                        DBGPRINTLN(String((p->packet[14] << 8) + p->packet[15]));
                        DBGPRINT(F("HW_FB_ReSPRT "));
                        DBGPRINTLN(String((p->packet[16] << 8) + p->packet[17]));
                        DBGPRINT(F("HW_GridSamp_ResValule "));
                        DBGPRINTLN(String((p->packet[18] << 8) + p->packet[19]));
                        DBGPRINT(F("HW_ECapValue "));
                        DBGPRINTLN(String((p->packet[20] << 8) + p->packet[21]));
                        DBGPRINT(F("Matching_APPFW_PN "));
                        DBGPRINTLN(String((uint32_t) (((p->packet[22] << 8) | p->packet[23]) << 8 | p->packet[24]) << 8 | p->packet[25]));
                    }
                    if(NULL != mCbPayload)
                        (mCbPayload)(InverterDevInform_All, q->iv);
                    q->iv->miMultiParts +=2;

                } else {
                    DBGPRINTLN(F("3rd gen. inverter!"));
                }

            } else if ( p->packet[9] == 0x12 ) {//3rd frame
                DPRINT_IVID(DBG_INFO, q->iv->id);
                DBGPRINTLN(F("got 3rd frame (hw info)"));
                /* according to xlsx (different start byte -1!)
                    byte[11]	 byte[12] APPFW_MINVER
                    byte[13]	 byte[14] HWInfoAddr
                    byte[15]	 byte[16] PNInfoCRC_gusv
                    byte[15]	 byte[16] PNInfoCRC_gusv (this really is double mentionned in xlsx...)
                */
                if(*mSerialDebug) {
                    DPRINT(DBG_INFO,F("APPFW_MINVER "));
                    DBGPRINTLN(String((p->packet[10] << 8) + p->packet[11]));
                    DBGPRINT(F("HWInfoAddr "));
                    DBGPRINTLN(String((p->packet[12] << 8) + p->packet[13]));
                    DBGPRINT(F("PNInfoCRC_gusv "));
                    DBGPRINTLN(String((p->packet[14] << 8) + p->packet[15]));
                }
                if(NULL != mCbPayload)
                    (mCbPayload)(InverterDevInform_Simple, q->iv);
                q->iv->miMultiParts++;
            }
            //if(q->iv->miMultiParts > 5)
                //closeRequest(q->iv, true);
            //else
            //if(q->iv->miMultiParts < 6)
            //    mState = States::WAIT;

            /*if (mPayload[iv->id].multi_parts > 5) {
                iv->setQueuedCmdFinished();
                mPayload[iv->id].complete = true;
                mPayload[iv->id].rxTmo    = true;
                mPayload[iv->id].requested= false;
                iv->radioStatistics.rxSuccess++;
            }
            if (mHighPrioIv == NULL)
                mHighPrioIv = iv;
                */
        }

        inline void miDataDecode(packet_t *p, const queue_s *q) {
            record_t<> *rec = q->iv->getRecordStruct(RealTimeRunData_Debug);  // choose the parser
            rec->ts = q->ts;
            //mState = States::RESET;
            if(q->iv->miMultiParts < 6)
                q->iv->miMultiParts += 6;

            uint8_t datachan = ( p->packet[0] == (MI_REQ_CH1 + ALL_FRAMES) || p->packet[0] == (MI_REQ_4CH + ALL_FRAMES) ) ? CH1 :
                           ( p->packet[0] == (MI_REQ_CH2 + ALL_FRAMES) || p->packet[0] == (0x37 + ALL_FRAMES) ) ? CH2 :
                           p->packet[0] == (0x38 + ALL_FRAMES) ? CH3 :
                           CH4;
            // count in RF_communication_protocol.xlsx is with offset = -1
            q->iv->setValue(q->iv->getPosByChFld(datachan, FLD_UDC, rec), rec, (float)((p->packet[9] << 8) + p->packet[10])/10);

            q->iv->setValue(q->iv->getPosByChFld(datachan, FLD_IDC, rec), rec, (float)((p->packet[11] << 8) + p->packet[12])/10);

            q->iv->setValue(q->iv->getPosByChFld(0, FLD_UAC, rec), rec, (float)((p->packet[13] << 8) + p->packet[14])/10);

            q->iv->setValue(q->iv->getPosByChFld(0, FLD_F, rec), rec, (float) ((p->packet[15] << 8) + p->packet[16])/100);
            q->iv->setValue(q->iv->getPosByChFld(datachan, FLD_PDC, rec), rec, (float)((p->packet[17] << 8) + p->packet[18])/10);

            q->iv->setValue(q->iv->getPosByChFld(datachan, FLD_YD, rec), rec, (float)((p->packet[19] << 8) + p->packet[20])/1);

            q->iv->setValue(q->iv->getPosByChFld(0, FLD_T, rec), rec, (float) ((int16_t)(p->packet[21] << 8) + p->packet[22])/10);
            q->iv->setValue(q->iv->getPosByChFld(0, FLD_IRR, rec), rec, (float) (calcIrradiation(q->iv, datachan)));

            if (datachan == 1)
                q->iv->rssi = p->rssi;
            else if(q->iv->rssi > p->rssi)
                q->iv->rssi = p->rssi;

            if (p->packet[0] >= (MI_REQ_4CH + ALL_FRAMES) ) {
                /*For MI1500:
                if (MI1500) {
                  STAT = (uint8_t)(p->packet[25] );
                  FCNT = (uint8_t)(p->packet[26]);
                  FCODE = (uint8_t)(p->packet[27]);
                }*/
                miStsConsolidate(q, datachan, rec, p->packet[23], p->packet[24]);

                if (p->packet[0] < (0x39 + ALL_FRAMES) ) {
                    mHeu.evalTxChQuality(q->iv, true, (4 - q->attempts), 1);
                    miNextRequest((p->packet[0] - ALL_FRAMES + 1), q);
                } else {
                    q->iv->miMultiParts = 7; // indicate we are ready
                    //miComplete(q->iv);
                }
            } else if((p->packet[0] == (MI_REQ_CH1 + ALL_FRAMES)) && (q->iv->type == INV_TYPE_2CH)) {
                //addImportant(q->iv, MI_REQ_CH2);
                miNextRequest(MI_REQ_CH2, q);
                mHeu.evalTxChQuality(q->iv, true, (4 - q->attempts), q->iv->curFrmCnt);
                //use also miMultiParts here for better statistics?
                //mHeu.setGotFragment(q->iv);
            } else {                                    // first data msg for 1ch, 2nd for 2ch
                q->iv->miMultiParts += 6; // indicate we are ready
                //miComplete(q->iv);
            }
        }

        void miNextRequest(uint8_t cmd, const queue_s *q) {
            incrAttempt();    // if function is called, we got something, and we necessarily need more transmissions for MI types...
            if(*mSerialDebug) {
                DPRINT_IVID(DBG_WARN, q->iv->id);
                DBGPRINT(F("next request ("));
                DBGPRINT(String(q->attempts));
                DBGPRINT(F(" attempts left): 0x"));
                DBGHEXLN(cmd);
            }

            if(q->iv->miMultiParts == 7) {
                //mHeu.setGotAll(q->iv);
                q->iv->radioStatistics.rxSuccess++;
            } else
                //mHeu.setGotFragment(q->iv);
                /*iv->radioStatistics.rxFail++; // got no complete payload*/
            //q->iv->radioStatistics.retransmits++;
            q->iv->radio->sendCmdPacket(q->iv, cmd, 0x00, true);

            mWaitTime.startTimeMonitor(MI_TIMEOUT);
            q->iv->miMultiParts = 0;
            q->iv->mGotFragment = 0;
            mIsRetransmit = true;
            chgCmd(cmd);
            //mState = States::WAIT;
        }

        void miRepeatRequest(const queue_s *q) {
            setAttempt();    // if function is called, we got something, and we necessarily need more transmissions for MI types...
            if(*mSerialDebug) {
                DPRINT_IVID(DBG_WARN, q->iv->id);
                DBGPRINT(F("resend request ("));
                DBGPRINT(String(q->attempts));
                DBGPRINT(F(" attempts left): 0x"));
                DBGHEXLN(q->cmd);
            }

            q->iv->radio->sendCmdPacket(q->iv, q->cmd, 0x00, true);

            mWaitTime.startTimeMonitor(MI_TIMEOUT);
            //mState = States::WAIT;
            mIsRetransmit = false;
        }

        void miStsConsolidate(const queue_s *q, uint8_t stschan,  record_t<> *rec, uint8_t uState, uint8_t uEnum, uint8_t lState = 0, uint8_t lEnum = 0) {
            //uint8_t status  = (p->packet[11] << 8) + p->packet[12];
            uint16_t statusMi = 3; // regular status for MI, change to 1 later?
            if ( uState == 2 ) {
                statusMi = 5050 + stschan; //first approach, needs review!
                if (lState)
                    statusMi +=  lState*10;
            } else if ( uState > 3 ) {
                statusMi = uState*1000 + uEnum*10;
                if (lState)
                    statusMi +=  lState*100; //needs review, esp. for 4ch-8310 state!
                //if (lEnum)
                statusMi +=  lEnum;
                if (uEnum < 6) {
                    statusMi += stschan;
                }
                if (statusMi == 8000)
                    statusMi = 8310;       //trick?
            }

            uint16_t prntsts = statusMi == 3 ? 1 : statusMi;
            bool stsok = true;
            if ( prntsts != rec->record[q->iv->getPosByChFld(0, FLD_EVT, rec)] ) { //sth.'s changed?
                q->iv->alarmCnt = 1; // minimum...
                stsok = false;
                //sth is or was wrong?
                if ( (q->iv->type != INV_TYPE_1CH) && ( (statusMi != 3)
                                                || ((q->iv->lastAlarm[stschan].code) && (statusMi == 3) && (q->iv->lastAlarm[stschan].code != 1)))
                   ) {
                    q->iv->lastAlarm[stschan+q->iv->type==INV_TYPE_2CH ? 2: 4] = alarm_t(q->iv->lastAlarm[stschan].code, q->iv->lastAlarm[stschan].start,q->ts);
                    q->iv->lastAlarm[stschan] = alarm_t(prntsts, q->ts,0);
                    q->iv->alarmCnt = q->iv->type == INV_TYPE_2CH ? 3 : 5;
                } else if ( (q->iv->type == INV_TYPE_1CH) && ( (statusMi != 3)
                                                || ((q->iv->lastAlarm[stschan].code) && (statusMi == 3) && (q->iv->lastAlarm[stschan].code != 1)))
                   ) {
                    q->iv->lastAlarm[stschan] = alarm_t(q->iv->lastAlarm[0].code, q->iv->lastAlarm[0].start,q->ts);
                } else if (q->iv->type == INV_TYPE_1CH)
                    stsok = true;

                q->iv->alarmLastId = prntsts; //iv->alarmMesIndex;

                if (q->iv->alarmCnt > 1) { //more than one channel
                    for (uint8_t ch = 0; ch < (q->iv->alarmCnt); ++ch) { //start with 1
                        if (q->iv->lastAlarm[ch].code == 1) {
                            stsok = true;
                            break;
                        }
                    }
                }
                if(*mSerialDebug) {
                    DPRINT(DBG_WARN, F("New state on CH"));
                    DBGPRINT(String(stschan)); DBGPRINT(F(" ("));
                    DBGPRINT(String(prntsts)); DBGPRINT(F("): "));
                    DBGPRINTLN(q->iv->getAlarmStr(prntsts));
                }
                if(!q->iv->miMultiParts)
                    q->iv->miMultiParts = 1; // indicate we got status info (1+2 ch types)
            }

            if (!stsok) {
                q->iv->setValue(q->iv->getPosByChFld(0, FLD_EVT, rec), rec, prntsts);
                q->iv->lastAlarm[0] = alarm_t(prntsts, q->ts, 0);
            }

            if (q->iv->alarmMesIndex < rec->record[q->iv->getPosByChFld(0, FLD_EVT, rec)]) {
                q->iv->alarmMesIndex = rec->record[q->iv->getPosByChFld(0, FLD_EVT, rec)]; // seems there's no status per channel in 3rd gen. models?!?
                if (*mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, q->iv->id);
                    DBGPRINT(F("alarm ID incremented to "));
                    DBGPRINTLN(String(q->iv->alarmMesIndex));
                }
            }
        }


        void miComplete(Inverter<> *iv) {
            if (*mSerialDebug) {
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINTLN(F("got all data msgs"));
            }
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, calcYieldDayCh0(iv,0));

            //preliminary AC calculation...
            float ac_pow = 0;
            if (iv->type == INV_TYPE_1CH) {
                if ((!iv->lastAlarm[0].code) || (iv->lastAlarm[0].code == 1))
                    ac_pow += iv->getValue(iv->getPosByChFld(1, FLD_PDC, rec), rec);
            } else {
                for(uint8_t i = 1; i <= iv->channels; i++) {
                    if ((!iv->lastAlarm[i].code) || (iv->lastAlarm[i].code == 1)) {
                        uint8_t pos = iv->getPosByChFld(i, FLD_PDC, rec);
                        ac_pow += iv->getValue(pos, rec);
                    }
                }
            }
            ac_pow = (int) (ac_pow*9.5);
            iv->setValue(iv->getPosByChFld(0, FLD_PAC, rec), rec, (float) ac_pow/10);

            iv->doCalculations();
            // update status state-machine,
            if (ac_pow)
                iv->isProducing();
            //closeRequest(iv, iv->miMultiParts > 5);

            //mHeu.setGotAll(iv);
            //cmdDone(false);
            if(NULL != mCbPayload)
                (mCbPayload)(RealTimeRunData_Debug, iv);

            //mState = States::RESET; // everything ok, next request
        }

    private:
        enum class States : uint8_t {
            RESET, START, WAIT, CHECK_FRAMES, CHECK_PACKAGE
        };

        typedef struct {
            uint8_t buf[MAX_RF_PAYLOAD_SIZE];
            uint8_t len;
            int8_t rssi;
        } frame_t;

    private:
        States mState = States::RESET;
        uint32_t *mTimestamp;
        bool *mPrivacyMode, *mSerialDebug, *mPrintWholeTrace;
        uint16_t *mInverterGap;
        TimeMonitor mWaitTime = TimeMonitor(0, true);  // start as expired (due to code in RESET state)
        std::array<frame_t, MAX_PAYLOAD_ENTRIES> mLocalBuf;
        bool mFirstTry = false;     // see, if we should do a second try
        bool mIsRetransmit = false; // we already had waited one complete cycle
        uint8_t mMaxFrameId;
        uint8_t mPayload[MAX_BUFFER];
        payloadListenerType mCbPayload = NULL;
        alarmListenerType mCbAlarm = NULL;
        Heuristic mHeu;
        uint32_t mLastEmptyQueueMillis = 0;
        bool mPrintSequenceDuration = false;

        //States mDebugState = States::START;
};

#endif /*__COMMUNICATION_H__*/
