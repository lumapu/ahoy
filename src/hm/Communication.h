//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#include "CommQueue.h"
#include <Arduino.h>
#include "../utils/crc.h"

class Communication : public CommQueue<> {
    public:
        void setup(uint32_t *timestamp) {
            mTimestamp = timestamp;
        }

        void loop() {
            get([this](bool valid, const queue_s *q) {
                if(!valid)
                    return; // empty

                switch(mState) {
                    case States::RESET:
                        lastFound = false;
                        for(uint8_t i = 0; i < MAX_PAYLOAD_ENTRIES; i++) {
                            mLocalBuf[i].len = 0;
                        }
                        mState = States::START;
                        break;

                    case States::START:
                        setTs(mTimestamp);
                        q->iv->radio->prepareDevInformCmd(q->iv, q->cmd, q->ts, q->iv->alarmLastId, false);
                        mWaitTimeout = millis() + 500;
                        setAttempt();
                        mState = States::WAIT;
                        break;

                    case States::WAIT:
                        if(millis() < mWaitTimeout)
                            return;
                        mState = States::CHECK_FRAMES;
                        break;

                    case States::CHECK_FRAMES: {
                        if(!q->iv->radio->loop()) {
                            pop();
                            q->iv->radioStatistics.rxFailNoAnser++; // got nothing
                            mState = States::RESET;
                            break; // radio buffer empty
                        }

                        States nextState = States::RESET;
                        while(!q->iv->radio->mBufCtrl.empty()) {
                            packet_t *p = &q->iv->radio->mBufCtrl.front();
                            q->iv->radio->mBufCtrl.pop();

                            DPRINT_IVID(DBG_INFO, q->iv->id);
                            DPRINT(DBG_INFO, F("RX "));
                            DBGPRINT(String(p->len));
                            DBGPRINT(F(" CH"));
                            DBGPRINT(String(p->ch));
                            DBGPRINT(F(", "));
                            DBGPRINT(String(p->rssi));
                            DBGPRINT(F("dBm | "));
                            ah::dumpBuf(p->packet, p->len);

                            if(!checkIvSerial(&p->packet[1], q->iv))
                                continue; // inverter ID incorrect

                            q->iv->radioStatistics.frmCnt++;

                            if (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES)) {  // response from get information command
                                parseFrame(p);
                                nextState = States::CHECK_PACKAGE;
                            } else if (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES)) // response from dev control command
                                parseDevCtrl(p);

                            yield();
                        }
                        mState = nextState;
                        }
                        break;

                    case States::CHECK_PACKAGE:
                        if(!lastFound) {
                            DPRINT_IVID(DBG_WARN, q->iv->id);
                            DBGPRINT(F("last frame "));
                            DBGPRINT(String(mMaxFrameId+1));
                            DBGPRINTLN(F(" missing: Request Retransmit"));

                            q->iv->radio->sendCmdPacket(q->iv, TX_REQ_INFO, (SINGLE_FRAME + mMaxFrameId + 1), true);
                            setAttempt();
                            mWaitTimeout = millis() + 100;
                            mState = States::WAIT;
                            break;
                        }

                        for(uint8_t i = 0; i < mMaxFrameId; i++) {
                            if(mLocalBuf[i].len == 0) {
                                DPRINT_IVID(DBG_WARN, q->iv->id);
                                DBGPRINT(F("frame "));
                                DBGPRINT(String(i + 1));
                                DBGPRINTLN(F(" missing: Request Retransmit"));

                                q->iv->radio->sendCmdPacket(q->iv, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                setAttempt();
                                mWaitTimeout = millis() + 100;
                                mState = States::WAIT;
                                return;
                            }
                        }

                        compilePayload(q);

                        pop(true); // remove done request
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
            if(0x00 == *frameId)
                return; // skip current packet
            if((*frameId & 0x7f) > MAX_PAYLOAD_ENTRIES) {
                DPRINTLN(DBG_WARN, F("local buffer to small for payload fragments!"));
                return; // local storage is to small for id
            }

            if(!checkFrameCrc(p->packet, p->len))
                return; // CRC8 is wrong, frame invalid

            if((*frameId & ALL_FRAMES) == ALL_FRAMES) {
                mMaxFrameId = (*frameId & 0x7f);
                if(*frameId > 0x81)
                    lastFound = true;
            }

            frame_t *f = &mLocalBuf[(*frameId & 0x7f) - 1];
            memcpy(f->buf, &p->packet[10], p->len-11);
            f->len  = p->len - 11;
            f->rssi = p->rssi;
        }

        inline void parseDevCtrl(packet_t *p) {
            //if((p->packet[12] == ActivePowerContr) && (p->packet[13] == 0x00))
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
                    pop();
                } else
                    DBGPRINTLN(F("-> complete retransmit"));
                mState = States::RESET;
                return;
            }

            DPRINT_IVID(DBG_INFO, q->iv->id);
            DBGPRINT(F("procPyld: cmd:  0x"));
            DBGHEXLN(q->cmd);

            memset(mPayload, 0, 150);
            int8_t rssi = -127;
            uint8_t len = 0;

            for(uint8_t i = 0; i < mMaxFrameId; i++) {
                if(mLocalBuf[i].len + len > 150) {
                    DPRINTLN(DBG_ERROR, F("payload buffer to small!"));
                    return;
                }
                memcpy(&mPayload[len], mLocalBuf[i].buf, mLocalBuf[i].len);
                len += mLocalBuf[i].len;
                // get worst RSSI
                if(mLocalBuf[i].rssi > rssi)
                    rssi = mLocalBuf[i].rssi;
            }

            len -= 2;

            DPRINT_IVID(DBG_INFO, q->iv->id);
            DBGPRINT(F("Payload ("));
            DBGPRINT(String(len));
            DBGPRINT(F("): "));
            ah::dumpBuf(mPayload, len);
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
        uint32_t mWaitTimeout;
        std::array<frame_t, MAX_PAYLOAD_ENTRIES> mLocalBuf;
        bool lastFound;
        uint8_t mMaxFrameId;
        uint8_t mPayload[150];
};

#endif /*__COMMUNICATION_H__*/
