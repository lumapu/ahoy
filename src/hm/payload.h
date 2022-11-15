//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __PAYLOAD_H__
#define __PAYLOAD_H__

#include "../utils/dbg.h"
#include "../utils/crc.h"
#include "../utils/handler.h"
#include <Arduino.h>

typedef struct {
    uint8_t txCmd;
    uint8_t txId;
    uint8_t invId;
    uint32_t ts;
    uint8_t data[MAX_PAYLOAD_ENTRIES][MAX_RF_PAYLOAD_SIZE];
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool complete;
    uint8_t maxPackId;
    uint8_t retransmits;
    bool requested;
} invPayload_t;


typedef std::function<void(uint8_t)> payloadListenerType;


template<class HMSYSTEM>
class Payload : public Handler<payloadListenerType> {
    public:
        Payload() : Handler() {}

        void setup(HMSYSTEM *sys) {
            mSys = sys;
            memset(mPayload, 0, (MAX_NUM_INVERTERS * sizeof(invPayload_t)));
            mLastPacketId = 0x00;
            mSerialDebug = false;
        }

        void enableSerialDebug(bool enable) {
            mSerialDebug = enable;
        }

        bool isComplete(Inverter<> *iv) {
            return mPayload[iv->id].complete;
        }

        uint8_t getMaxPacketId(Inverter<> *iv) {
            return mPayload[iv->id].maxPackId;
        }

        uint8_t getRetransmits(Inverter<> *iv) {
            return mPayload[iv->id].retransmits;
        }

        uint32_t getTs(Inverter<> *iv) {
            return mPayload[iv->id].ts;
        }

        void request(Inverter<> *iv) {
            mPayload[iv->id].requested = true;
        }

        void setTxCmd(Inverter<> *iv, uint8_t cmd) {
            mPayload[iv->id].txCmd = cmd;
        }

        void notify(uint8_t val) {
            for(typename std::list<payloadListenerType>::iterator it = mList.begin(); it != mList.end(); ++it) {
               (*it)(val);
            }
        }

        void add(packet_t *p, uint8_t len) {
            Inverter<> *iv = mSys->findInverter(&p->packet[1]);
            if ((NULL != iv) && (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES))) {  // response from get information command
                mPayload[iv->id].txId = p->packet[0];
                DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                uint8_t *pid = &p->packet[9];
                if (*pid == 0x00) {
                    DPRINT(DBG_DEBUG, F("fragment number zero received and ignored"));
                } else {
                    DPRINTLN(DBG_DEBUG, "PID: 0x" + String(*pid, HEX));
                    if ((*pid & 0x7F) < 5) {
                        memcpy(mPayload[iv->id].data[(*pid & 0x7F) - 1], &p->packet[10], len - 11);
                        mPayload[iv->id].len[(*pid & 0x7F) - 1] = len - 11;
                    }

                    if ((*pid & ALL_FRAMES) == ALL_FRAMES) {
                        // Last packet
                        if ((*pid & 0x7f) > mPayload[iv->id].maxPackId) {
                            mPayload[iv->id].maxPackId = (*pid & 0x7f);
                            if (*pid > 0x81)
                                mLastPacketId = *pid;
                        }
                    }
                }
            }
            if ((NULL != iv) && (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES))) { // response from dev control command
                DPRINTLN(DBG_DEBUG, F("Response from devcontrol request received"));

                mPayload[iv->id].txId = p->packet[0];
                iv->devControlRequest = false;

                if ((p->packet[12] == ActivePowerContr) && (p->packet[13] == 0x00)) {
                    String msg = (p->packet[10] == 0x00 && p->packet[11] == 0x00) ? "" : "NOT ";
                    DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(" has ") + msg + F("accepted power limit set point ") + String(iv->powerLimit[0]) + F(" with PowerLimitControl ") + String(iv->powerLimit[1]));
                }
                iv->devControlCmd = Init;
            }
        }

        bool build(uint8_t id) {
            DPRINTLN(DBG_VERBOSE, F("build"));
            uint16_t crc = 0xffff, crcRcv = 0x0000;
            if (mPayload[id].maxPackId > MAX_PAYLOAD_ENTRIES)
                mPayload[id].maxPackId = MAX_PAYLOAD_ENTRIES;

            for (uint8_t i = 0; i < mPayload[id].maxPackId; i++) {
                if (mPayload[id].len[i] > 0) {
                    if (i == (mPayload[id].maxPackId - 1)) {
                        crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i] - 2, crc);
                        crcRcv = (mPayload[id].data[i][mPayload[id].len[i] - 2] << 8) | (mPayload[id].data[i][mPayload[id].len[i] - 1]);
                    } else
                        crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i], crc);
                }
                yield();
            }

            return (crc == crcRcv) ? true : false;
        }

        void process(bool retransmit, uint8_t maxRetransmits, statistics_t *stat) {
            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter

                if ((mPayload[iv->id].txId != (TX_REQ_INFO + ALL_FRAMES)) && (0 != mPayload[iv->id].txId)) {
                    // no processing needed if txId is not 0x95
                    // DPRINTLN(DBG_INFO, F("processPayload - set complete, txId: ") + String(mPayload[iv->id].txId, HEX));
                    mPayload[iv->id].complete = true;
                }

                if (!mPayload[iv->id].complete) {
                    if (!build(iv->id)) { // payload not complete
                        if ((mPayload[iv->id].requested) && (retransmit)) {
                            if (iv->devControlCmd == Restart || iv->devControlCmd == CleanState_LockAndAlarm) {
                                // This is required to prevent retransmissions without answer.
                                DPRINTLN(DBG_INFO, F("Prevent retransmit on Restart / CleanState_LockAndAlarm..."));
                                mPayload[iv->id].retransmits = maxRetransmits;
                            } else {
                                if (mPayload[iv->id].retransmits < maxRetransmits) {
                                    mPayload[iv->id].retransmits++;
                                    if (mPayload[iv->id].maxPackId != 0) {
                                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                            if (mPayload[iv->id].len[i] == 0) {
                                                DPRINTLN(DBG_WARN, F("while retrieving data: Frame ") + String(i + 1) + F(" missing: Request Retransmit"));
                                                mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                                break;  // only retransmit one frame per loop
                                            }
                                            yield();
                                        }
                                    } else {
                                        DPRINTLN(DBG_WARN, F("while retrieving data: last frame missing: Request Retransmit"));
                                        if (0x00 != mLastPacketId)
                                            mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, mLastPacketId, true);
                                        else {
                                            mPayload[iv->id].txCmd = iv->getQueuedCmd();
                                            DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") sendTimePacket"));
                                            mSys->Radio.sendTimePacket(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex);
                                        }
                                    }
                                    mSys->Radio.switchRxCh(100);
                                }
                            }
                        }
                    } else {  // payload complete
                        DPRINTLN(DBG_INFO, F("procPyld: cmd:  ") + String(mPayload[iv->id].txCmd));
                        DPRINTLN(DBG_INFO, F("procPyld: txid: 0x") + String(mPayload[iv->id].txId, HEX));
                        DPRINTLN(DBG_DEBUG, F("procPyld: max:  ") + String(mPayload[iv->id].maxPackId));
                        record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                        mPayload[iv->id].complete = true;

                        uint8_t payload[128];
                        uint8_t payloadLen = 0;

                        memset(payload, 0, 128);

                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i++) {
                            memcpy(&payload[payloadLen], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                            payloadLen += (mPayload[iv->id].len[i]);
                            yield();
                        }
                        payloadLen -= 2;

                        if (mSerialDebug) {
                            DPRINT(DBG_INFO, F("Payload (") + String(payloadLen) + "): ");
                            mSys->Radio.dumpBuf(NULL, payload, payloadLen);
                        }

                        if (NULL == rec) {
                            DPRINTLN(DBG_ERROR, F("record is NULL!"));
                        } else if ((rec->pyldLen == payloadLen) || (0 == rec->pyldLen)) {
                            if (mPayload[iv->id].txId == (TX_REQ_INFO + 0x80))
                                stat->rxSuccess++;

                            rec->ts = mPayload[iv->id].ts;
                            for (uint8_t i = 0; i < rec->length; i++) {
                                iv->addValue(i, payload, rec);
                                yield();
                            }
                            iv->doCalculations();
                            notify(mPayload[iv->id].txCmd);
                        } else {
                            DPRINTLN(DBG_ERROR, F("plausibility check failed, expected ") + String(rec->pyldLen) + F(" bytes"));
                            stat->rxFail++;
                        }

                        iv->setQueuedCmdFinished();
                    }
                }

                yield();

            }
        }

        void reset(Inverter<> *iv, uint32_t utcTs) {
            DPRINTLN(DBG_INFO, "resetPayload: id: " + String(iv->id));
            memset(mPayload[iv->id].len, 0, MAX_PAYLOAD_ENTRIES);
            mPayload[iv->id].txCmd = 0;
            mPayload[iv->id].retransmits = 0;
            mPayload[iv->id].maxPackId = 0;
            mPayload[iv->id].complete = false;
            mPayload[iv->id].requested = false;
            mPayload[iv->id].ts = utcTs;
        }

    private:
        HMSYSTEM *mSys;
        invPayload_t mPayload[MAX_NUM_INVERTERS];
        uint8_t mLastPacketId;
        bool mSerialDebug;
};

#endif /*__PAYLOAD_H_*/
