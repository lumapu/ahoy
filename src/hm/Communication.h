//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#include "CommQueue.h"
#include <Arduino.h>
#include "../utils/crc.h"

#define MI_TIMEOUT      250
#define DEFAULT_TIMEOUT 500
#define SINGLEFR_TIMEOUT 60
#define MAX_BUFFER      250 //was: 150 (hardcoded)

typedef std::function<void(uint8_t, Inverter<> *)> payloadListenerType;
typedef std::function<void(Inverter<> *)> alarmListenerType;

class Communication : public CommQueue<> {
    public:
        void setup(uint32_t *timestamp) {
            mTimestamp = timestamp;
        }

        void addImportant(Inverter<> *iv, uint8_t cmd, bool delOnPop = true) {
            mState = States::RESET; // cancel current operation
            CommQueue::addImportant(iv, cmd, delOnPop);
        }

        void addPayloadListener(payloadListenerType cb) {
            mCbPayload = cb;
        }

        void addAlarmListener(alarmListenerType cb) {
            mCbAlarm = cb;
        }

        void loop() {
            get([this](bool valid, const queue_s *q) {
                if(!valid)
                    return; // empty

                uint16_t timeout = q->iv->ivGen != IV_MI ? DEFAULT_TIMEOUT : MI_TIMEOUT;

                switch(mState) {
                    case States::RESET:
                        if(millis() < mWaitTimeout)
                            return;
                        mMaxFrameId = 0;
                        for(uint8_t i = 0; i < MAX_PAYLOAD_ENTRIES; i++) {
                            mLocalBuf[i].len = 0;
                        }
                        mState = States::START;
                        break;

                    case States::START:
                        setTs(mTimestamp);
                        if(q->isDevControl) {
                            if(ActivePowerContr == q->cmd)
                                q->iv->powerLimitAck = false;
                            q->iv->radio->sendControlPacket(q->iv, q->cmd, q->iv->powerLimit, false);
                        } else
                            q->iv->radio->prepareDevInformCmd(q->iv, q->cmd, q->ts, q->iv->alarmLastId, false);
                        q->iv->radioStatistics.txCnt++;
                        mWaitTimeout = millis() + timeout;
                        setAttempt();
                        mState = States::WAIT;
                        break;

                    case States::WAIT:
                        if(millis() < mWaitTimeout)
                            return;
                        mState = States::CHECK_FRAMES;
                        break;

                    case States::CHECK_FRAMES: {
                        if(!q->iv->radio->get()) { // radio buffer empty
                            cmdDone();
                            DPRINT_IVID(DBG_INFO, q->iv->id);
                            DBGPRINT(F("request timeout: "));
                            DBGPRINT(String(millis() - mWaitTimeout + timeout));
                            DBGPRINTLN(F("ms"));

                            q->iv->radioStatistics.rxFailNoAnser++; // got nothing
                            if((IV_HMS == q->iv->ivGen) || (IV_HMT == q->iv->ivGen)) {
                                q->iv->radio->switchFrequency(q->iv, HOY_BOOT_FREQ_KHZ, WORK_FREQ_KHZ);
                                mWaitTimeout = millis() + 1000;
                            }
                            mState = States::RESET;
                            break;
                        }

                        States nextState = States::RESET;
                        while(!q->iv->radio->mBufCtrl.empty()) {
                            packet_t *p = &q->iv->radio->mBufCtrl.front();

                            DPRINT_IVID(DBG_INFO, q->iv->id);
                            DBGPRINT(F("RX "));
                            if(p->millis < 100)
                                DBGPRINT(F(" "));
                            DBGPRINT(String(p->millis));
                            DBGPRINT(F("ms "));
                            DBGPRINT(String(p->len));
                            if((IV_HM == q->iv->ivGen) || (IV_MI == q->iv->ivGen)) {
                                DBGPRINT(F(" CH"));
                                if(3 == p->ch)
                                    DBGPRINT(F("0"));
                                DBGPRINT(String(p->ch));
                            }
                            DBGPRINT(F(", "));
                            DBGPRINT(String(p->rssi));
                            DBGPRINT(F("dBm | "));
                            ah::dumpBuf(p->packet, p->len);

                            if(checkIvSerial(&p->packet[1], q->iv)) {
                                q->iv->radioStatistics.frmCnt++;

                                if (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES)) {  // response from get information command
                                    parseFrame(p);
                                    nextState = States::CHECK_PACKAGE;
                                } else if (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES)) { // response from dev control command
                                    parseDevCtrl(p, q);
                                    cmdDone(true); // remove done request
                                } else if(IV_MI == q->iv->ivGen) {
                                    parseMiFrame(p, q);
                                }
                            } else {
                                DPRINTLN(DBG_WARN, F("Inverter serial does not match"));
                                mWaitTimeout = millis() + timeout;
                            }

                            q->iv->radio->mBufCtrl.pop();
                            yield();
                        }
                        if(0 == q->attempts) {
                            //cmdDone(q);
                            cmdDone(true);
                            mState = States::RESET;
                        } else
                            mState = nextState;

                        }
                        break;

                    case States::CHECK_PACKAGE:
                        if(0 == mMaxFrameId) {
                            setAttempt();

                            DPRINT_IVID(DBG_WARN, q->iv->id);
                            DBGPRINT(F("frame missing: request retransmit ("));
                            DBGPRINT(String(q->attempts));
                            DBGPRINTLN(F(" attempts left)"));

                            uint8_t i = 0;
                            while(i < MAX_PAYLOAD_ENTRIES) {
                                if(mLocalBuf[i].len == 0)
                                    break;
                                i++;
                            }

                            if(q->attempts) {
                                q->iv->radio->sendCmdPacket(q->iv, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                q->iv->radioStatistics.retransmits++;
                                mWaitTimeout = millis() + SINGLEFR_TIMEOUT; // timeout
                                mState = States::WAIT;
                            } else {
                                add(q, true);
                                //cmdDone(q);
                                cmdDone(true);
                                mState = States::RESET;
                            }
                            return;
                        }

                        for(uint8_t i = 0; i < mMaxFrameId; i++) {
                            if(mLocalBuf[i].len == 0) {
                                setAttempt();

                                DPRINT_IVID(DBG_WARN, q->iv->id);
                                DBGPRINT(F("frame "));
                                DBGPRINT(String(i + 1));
                                DBGPRINT(F(" missing: request retransmit ("));
                                DBGPRINT(String(q->attempts));
                                DBGPRINTLN(F(" attempts left)"));

                                if(q->attempts) {
                                    q->iv->radio->sendCmdPacket(q->iv, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                    q->iv->radioStatistics.retransmits++;
                                    mWaitTimeout = millis() + SINGLEFR_TIMEOUT; // timeout;
                                    mState = States::WAIT;
                                } else {
                                    add(q, true);
                                    //cmdDone(q);
                                    cmdDone(true);
                                    mState = States::RESET;
                                }
                                return;
                            }
                        }

                        compilePayload(q);

                        if(NULL != mCbPayload)
                            (mCbPayload)(q->cmd, q->iv);

                        cmdDone(true); // remove done request
                        mState = States::RESET; // everything ok, next request
                        break;
                }
            });
        }

    private:
        inline bool checkIvSerial(uint8_t buf[], Inverter<> *iv) {
            uint8_t tmp[4];
            CP_U32_BigEndian(tmp, iv->radioId.u64 >> 8);
            for(uint8_t i = 0; i < 4; i++) {
                if(tmp[i] != buf[i])
                    return false;
            }
            return true;
        }

        inline bool checkFrameCrc(uint8_t buf[], uint8_t len) {
            return (ah::crc8(buf, len - 1) == buf[len-1]);
        }

        inline void parseFrame(packet_t *p) {
            uint8_t *frameId = &p->packet[9];
            if(0x00 == *frameId) {
                DPRINTLN(DBG_WARN, F("invalid frameId 0x00"));
                return; // skip current packet
            }
            if((*frameId & 0x7f) > MAX_PAYLOAD_ENTRIES) {
                DPRINTLN(DBG_WARN, F("local buffer to small for payload fragments"));
                return; // local storage is to small for id
            }

            if(!checkFrameCrc(p->packet, p->len)) {
                DPRINTLN(DBG_WARN, F("frame CRC is wrong"));
                return; // CRC8 is wrong, frame invalid
            }

            if((*frameId & ALL_FRAMES) == ALL_FRAMES)
                mMaxFrameId = (*frameId & 0x7f);

            frame_t *f = &mLocalBuf[(*frameId & 0x7f) - 1];
            memcpy(f->buf, &p->packet[10], p->len-11);
            f->len  = p->len - 11;
            f->rssi = p->rssi;
        }

        inline void parseMiFrame(packet_t *p, const queue_s *q) {
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
            }
        }

        inline void parseDevCtrl(packet_t *p, const queue_s *q) {
            if((p->packet[12] != ActivePowerContr) || (p->packet[13] != 0x00))
                return;
            bool accepted = true;
            if((p->packet[10] == 0x00) && (p->packet[11] == 0x00))
                q->iv->powerLimitAck = true;
            else
                accepted = false;

            DPRINT_IVID(DBG_INFO, q->iv->id);
            DBGPRINT(F(" has "));
            if(!accepted) DBGPRINT(F("not "));
            DBGPRINT(F("accepted power limit set point "));
            DBGPRINT(String(q->iv->powerLimit[0]));
            DBGPRINT(F(" with PowerLimitControl "));
            DBGPRINTLN(String(q->iv->powerLimit[1]));
            q->iv->actPowerLimit = 0xffff; // unknown, readback current value
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
                    q->iv->radioStatistics.rxFail++; // got fragments but not complete response
                    cmdDone();
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
            DBGPRINT(F("): "));
            ah::dumpBuf(mPayload, len);

            record_t<> *rec = q->iv->getRecordStruct(q->cmd);
            if(NULL == rec) {
                DPRINTLN(DBG_ERROR, F("record is NULL!"));
                return;
            }
            if((rec->pyldLen != len) && (0 != rec->pyldLen)) {
                DPRINT(DBG_ERROR, F("plausibility check failed, expected "));
                DBGPRINT(String(rec->pyldLen));
                DBGPRINTLN(F(" bytes"));
                q->iv->radioStatistics.rxFail++;
                return;
            }

            q->iv->radioStatistics.rxSuccess++;

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

    private:
        inline void miHwDecode(packet_t *p, const queue_s *q) {
            record_t<> *rec = q->iv->getRecordStruct(InverterDevInform_All);  // choose the record structure
            rec->ts = q->ts;
            //mPayload[iv->id].gotFragment = true;
            uint8_t multi_parts = 0;

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
                //if(mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, q->iv->id);
                    DPRINT(DBG_INFO,F("HW_VER is "));
                    DBGPRINTLN(String((p->packet[24] << 8) + p->packet[25]));
                //}
                record_t<> *rec = q->iv->getRecordStruct(InverterDevInform_Simple);  // choose the record structure
                rec->ts = q->ts;
                q->iv->setValue(1, rec, (uint32_t) ((p->packet[24] << 8) + p->packet[25])/1);
                //mPayload[iv->id].multi_parts +=4;
                multi_parts +=4;
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

                    //if(mSerialDebug) {
                        DPRINT(DBG_INFO,F("HW_FB_TLmValue "));
                        DBGPRINTLN(String((p->packet[14] << 8) + p->packet[15]));
                        DPRINT(DBG_INFO,F("HW_FB_ReSPRT "));
                        DBGPRINTLN(String((p->packet[16] << 8) + p->packet[17]));
                        DPRINT(DBG_INFO,F("HW_GridSamp_ResValule "));
                        DBGPRINTLN(String((p->packet[18] << 8) + p->packet[19]));
                        DPRINT(DBG_INFO,F("HW_ECapValue "));
                        DBGPRINTLN(String((p->packet[20] << 8) + p->packet[21]));
                        DPRINT(DBG_INFO,F("Matching_APPFW_PN "));
                        DBGPRINTLN(String((uint32_t) (((p->packet[22] << 8) | p->packet[23]) << 8 | p->packet[24]) << 8 | p->packet[25]));
                    //}
                    //notify(InverterDevInform_Simple, iv);
                    //mPayload[iv->id].multi_parts +=2;
                    multi_parts +=2;
                    //notify(InverterDevInform_All, iv);
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
                //if(mSerialDebug) {
                    DPRINT(DBG_INFO,F("APPFW_MINVER "));
                    DBGPRINTLN(String((p->packet[10] << 8) + p->packet[11]));
                    DPRINT(DBG_INFO,F("HWInfoAddr "));
                    DBGPRINTLN(String((p->packet[12] << 8) + p->packet[13]));
                    DPRINT(DBG_INFO,F("PNInfoCRC_gusv "));
                    DBGPRINTLN(String((p->packet[14] << 8) + p->packet[15]));
                //}
                //mPayload[iv->id].multi_parts++;
                multi_parts++;
            }
            if(multi_parts > 5) {
                cmdDone(true);
                mState = States::RESET;
                q->iv->radioStatistics.rxSuccess++;
            }

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
            q->iv->radioStatistics.rxSuccess++;
            mState = States::RESET;

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
            //mPayload[q->iv->id].rssi[(datachan-1)] = p->rssi;

            if (datachan == 1) //mPayload[q->iv->id].rssi[(datachan-1)] = p->rssi;
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
                    //addImportant(q->iv, (q->cmd + 1));
                    //mPayload[iv->id].txCmd++;
                    //mPayload[iv->id].retransmits = 0; // reserve retransmissions for each response
                    //mPayload[iv->id].complete = false;
                    miNextRequest((p->packet[0] - ALL_FRAMES + 1), q);
                } else {
                    miComplete(q->iv);
                }
            } else if((p->packet[0] == (MI_REQ_CH1 + ALL_FRAMES)) && (q->iv->type == INV_TYPE_2CH)) {
                //addImportant(q->iv, MI_REQ_CH2);
                miNextRequest(MI_REQ_CH2, q);
            } else {                                    // first data msg for 1ch, 2nd for 2ch
                miComplete(q->iv);
            }
        }

        inline void miNextRequest(uint8_t cmd, const queue_s *q) {
            //setAttempt();
            DPRINT_IVID(DBG_WARN, q->iv->id);
            DBGPRINT(F("next request ("));
            DBGPRINT(String(q->attempts));
            DBGPRINT(F(" attempts left): 0x"));
            DBGHEXLN(cmd);

            if(q->attempts) {
                q->iv->radio->sendCmdPacket(q->iv, cmd, 0x00, true);
                q->iv->radioStatistics.retransmits++;
                mWaitTimeout = millis() + MI_TIMEOUT;
                //chgCmd(Inverter<> *iv, uint8_t cmd, bool delOnPop = true)
                chgCmd(cmd);
                mState = States::WAIT;
            } else {
                add(q, true);
                cmdDone();
                mState = States::RESET;
            }
        }

        inline void miStsConsolidate(const queue_s *q, uint8_t stschan,  record_t<> *rec, uint8_t uState, uint8_t uEnum, uint8_t lState = 0, uint8_t lEnum = 0) {
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
                //if (mSerialDebug) {
                    DPRINT(DBG_WARN, F("New state on CH"));
                    DBGPRINT(String(stschan)); DBGPRINT(F(" ("));
                    DBGPRINT(String(prntsts)); DBGPRINT(F("): "));
                    DBGPRINTLN(q->iv->getAlarmStr(prntsts));
                //}
            }

            if (!stsok) {
                q->iv->setValue(q->iv->getPosByChFld(0, FLD_EVT, rec), rec, prntsts);
                q->iv->lastAlarm[0] = alarm_t(prntsts, q->ts, 0);
            }

            if (q->iv->alarmMesIndex < rec->record[q->iv->getPosByChFld(0, FLD_EVT, rec)]) {
                q->iv->alarmMesIndex = rec->record[q->iv->getPosByChFld(0, FLD_EVT, rec)]; // seems there's no status per channel in 3rd gen. models?!?
                //if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, q->iv->id);
                    DBGPRINT(F("alarm ID incremented to "));
                    DBGPRINTLN(String(q->iv->alarmMesIndex));
                //}
            }
        }


        inline void miComplete(Inverter<> *iv) {
            //if ( mPayload[iv->id].complete )
            //    return; //if we got second message as well in repreated attempt
            //mPayload[iv->id].complete = true;
            //if (mSerialDebug) {
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINTLN(F("got all data msgs"));
            //}
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
            cmdDone(true);
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
        uint32_t mWaitTimeout = 0;
        std::array<frame_t, MAX_PAYLOAD_ENTRIES> mLocalBuf;
        uint8_t mMaxFrameId;
        uint8_t mPayload[MAX_BUFFER];
        payloadListenerType mCbPayload = NULL;
        alarmListenerType mCbAlarm = NULL;
};

#endif /*__COMMUNICATION_H__*/
