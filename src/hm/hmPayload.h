//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HM_PAYLOAD_H__
#define __HM_PAYLOAD_H__

#include "../utils/dbg.h"
#include "../utils/crc.h"
#include "../config/config.h"
#include "hmRadio.h"
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
    bool lastFound;
    uint8_t retransmits;
    bool requested;
    bool gotFragment;
} invPayload_t;


typedef std::function<void(uint8_t, Inverter<> *)> payloadListenerType;
typedef std::function<void(Inverter<> *)> alarmListenerType;


template<class HMSYSTEM, class HMRADIO>
class HmPayload {
    public:
        HmPayload() {}

        void setup(IApp *app, HMSYSTEM *sys, HMRADIO *radio, statistics_t *stat, uint8_t maxRetransmits, uint32_t *timestamp) {
            mApp        = app;
            mSys        = sys;
            mRadio      = radio;
            mStat       = stat;
            mMaxRetrans = maxRetransmits;
            mTimestamp  = timestamp;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                reset(i);
            }
            mSerialDebug  = false;
            mHighPrioIv   = NULL;
            mCbAlarm      = NULL;
            mCbPayload    = NULL;
        }

        void enableSerialDebug(bool enable) {
            mSerialDebug = enable;
        }

        void addPayloadListener(payloadListenerType cb) {
            mCbPayload = cb;
        }

        void addAlarmListener(alarmListenerType cb) {
            mCbAlarm = cb;
        }

        void loop() {
            if (NULL != mHighPrioIv) {
                ivSend(mHighPrioIv, true);
                mHighPrioIv = NULL;
            }
        }

        /*void simulation() {
            uint8_t pay[] = {
                0x00, 0x01, 0x01, 0x24, 0x02, 0x28, 0x02, 0x33,
                0x06, 0x49, 0x06, 0x6a, 0x00, 0x05, 0x5f, 0x1b,
                0x00, 0x06, 0x66, 0x9a, 0x03, 0xfd, 0x04, 0x0b,
                0x01, 0x23, 0x02, 0x28, 0x02, 0x28, 0x06, 0x41,
                0x06, 0x43, 0x00, 0x05, 0xdc, 0x2c, 0x00, 0x06,
                0x2e, 0x3f, 0x04, 0x01, 0x03, 0xfb, 0x09, 0x78,
                0x13, 0x86, 0x18, 0x15, 0x00, 0xcf, 0x00, 0xfe,
                0x03, 0xe7, 0x01, 0x42, 0x00, 0x03
            };

            Inverter<> *iv = mSys->getInverterByPos(0);
            record_t<> *rec = iv->getRecordStruct(0x0b);
            rec->ts = *mTimestamp;
            for (uint8_t i = 0; i < rec->length; i++) {
                iv->addValue(i, pay, rec);
                yield();
            }
            iv->doCalculations();
            notify(0x0b, iv);
        }*/

        void ivSendHighPrio(Inverter<> *iv) {
            mHighPrioIv = iv;
        }

        void ivSend(Inverter<> *iv, bool highPrio = false) {
            if(!highPrio) {
                if (mPayload[iv->id].requested) {
                    if (!mPayload[iv->id].complete)
                        process(false); // no retransmit

                    if (!mPayload[iv->id].complete) {
                        if (mSerialDebug)
                            DPRINT_IVID(DBG_INFO, iv->id);
                        if (MAX_PAYLOAD_ENTRIES == mPayload[iv->id].maxPackId) {
                            mStat->rxFailNoAnser++; // got nothing
                            if (mSerialDebug)
                                DBGPRINTLN(F("enqueued cmd failed/timeout"));
                        } else {
                            mStat->rxFail++; // got fragments but not complete response
                            if (mSerialDebug) {
                                DBGPRINT(F("no complete Payload received! (retransmits: "));
                                DBGPRINT(String(mPayload[iv->id].retransmits));
                                DBGPRINTLN(F(")"));
                            }
                        }
                        iv->setQueuedCmdFinished();  // command failed
                    }
                }
            }

            reset(iv->id);
            mPayload[iv->id].requested = true;

            yield();
            if (mSerialDebug) {
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINT(F("Requesting Inv SN "));
                DBGPRINTLN(String(iv->config->serial.u64, HEX));
            }

            if (iv->getDevControlRequest()) {
                if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F("Devcontrol request 0x"));
                    DBGPRINT(String(iv->devControlCmd, HEX));
                    DBGPRINT(F(" power limit "));
                    DBGPRINTLN(String(iv->powerLimit[0]));
                }
                iv->powerLimitAck = false;
                mRadio->sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, false);
                mPayload[iv->id].txCmd = iv->devControlCmd;
                //iv->clearCmdQueue();
                //iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
            } else {
                uint8_t cmd = iv->getQueuedCmd();
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINT(F("prepareDevInformCmd 0x"));
                DBGHEXLN(cmd);
                mRadio->prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false);
                mPayload[iv->id].txCmd = cmd;
            }
        }

        void add(Inverter<> *iv, packet_t *p) {
            if (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES)) {  // response from get information command
                mPayload[iv->id].txId = p->packet[0];
                DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                uint8_t *pid = &p->packet[9];
                if (*pid == 0x00) {
                    DPRINTLN(DBG_DEBUG, F("fragment number zero received and ignored"));
                } else {
                    DPRINT(DBG_DEBUG, F("PID: 0x"));
                    DPRINTLN(DBG_DEBUG, String(*pid, HEX));
                    if ((*pid & 0x7F) < MAX_PAYLOAD_ENTRIES) {
                        memcpy(mPayload[iv->id].data[(*pid & 0x7F) - 1], &p->packet[10], p->len - 11);
                        mPayload[iv->id].len[(*pid & 0x7F) - 1] = p->len - 11;
                        mPayload[iv->id].gotFragment = true;
                    }

                    if ((*pid & ALL_FRAMES) == ALL_FRAMES) {
                        // Last packet
                        if (((*pid & 0x7f) > mPayload[iv->id].maxPackId) || (MAX_PAYLOAD_ENTRIES == mPayload[iv->id].maxPackId)) {
                            mPayload[iv->id].maxPackId = (*pid & 0x7f);
                            if (*pid > 0x81)
                                mPayload[iv->id].lastFound = true;
                        }
                    }
                }
            } else if (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES)) { // response from dev control command
                DPRINTLN(DBG_DEBUG, F("Response from devcontrol request received"));

                mPayload[iv->id].txId = p->packet[0];
                iv->clearDevControlRequest();

                if ((p->packet[12] == ActivePowerContr) && (p->packet[13] == 0x00)) {
                    bool ok = true;
                    if((p->packet[10] == 0x00) && (p->packet[11] == 0x00)) {
                        mApp->setMqttPowerLimitAck(iv);
                        iv->powerLimitAck = true;
                    } else
                        ok = false;

                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F(" has "));
                    if(!ok) DBGPRINT(F("not "));
                    DBGPRINT(F("accepted power limit set point "));
                    DBGPRINT(String(iv->powerLimit[0]));
                    DBGPRINT(F(" with PowerLimitControl "));
                    DBGPRINTLN(String(iv->powerLimit[1]));

                    iv->clearCmdQueue();
                    iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
                    if(mHighPrioIv == NULL)                          // do it immediately if possible
                        mHighPrioIv = iv;
                }
                iv->devControlCmd = Init;
            }
        }

        void process(bool retransmit) {
            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter

                if (IV_HM != iv->ivGen) // only process HM inverters
                    continue; // skip to next inverter

                if ((mPayload[iv->id].txId != (TX_REQ_INFO + ALL_FRAMES)) && (0 != mPayload[iv->id].txId)) {
                    // no processing needed if txId is not 0x95
                    mPayload[iv->id].complete = true;
                    continue; // skip to next inverter
                }

                if (!mPayload[iv->id].complete) {
                    bool crcPass, pyldComplete;
                    crcPass = build(iv->id, &pyldComplete);
                    if (!crcPass && !pyldComplete) { // payload not complete
                        if ((mPayload[iv->id].requested) && (retransmit)) {
                            if (mPayload[iv->id].retransmits < mMaxRetrans) {
                                mPayload[iv->id].retransmits++;
                                if (iv->devControlCmd == Restart || iv->devControlCmd == CleanState_LockAndAlarm) {
                                    // This is required to prevent retransmissions without answer.
                                    DPRINTLN(DBG_INFO, F("Prevent retransmit on Restart / CleanState_LockAndAlarm..."));
                                    mPayload[iv->id].retransmits = mMaxRetrans;
                                } else if(iv->devControlCmd == ActivePowerContr) {
                                    DPRINT_IVID(DBG_INFO, iv->id);
                                    DPRINTLN(DBG_INFO, F("retransmit power limit"));
                                    mRadio->sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, true);
                                } else {
                                    if(false == mPayload[iv->id].gotFragment) {
                                        /*
                                        DPRINTLN(DBG_WARN, F("nothing received: Request Complete Retransmit"));
                                        mPayload[iv->id].txCmd = iv->getQueuedCmd();
                                        DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") prepareDevInformCmd 0x") + String(mPayload[iv->id].txCmd, HEX));
                                        mRadio->prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex, true);
                                        */
                                        DPRINT_IVID(DBG_INFO, iv->id);
                                        DBGPRINTLN(F("nothing received"));
                                        mPayload[iv->id].retransmits = mMaxRetrans;
                                    } else {
                                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                            if (mPayload[iv->id].len[i] == 0) {
                                                DPRINT_IVID(DBG_WARN, iv->id);
                                                DBGPRINT(F("Frame "));
                                                DBGPRINT(String(i + 1));
                                                DBGPRINTLN(F(" missing: Request Retransmit"));
                                                mRadio->sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                                break;  // only request retransmit one frame per loop
                                            }
                                            yield();
                                        }
                                    }
                                }
                            }
                        }
                    } else if(!crcPass && pyldComplete) { // crc error on complete Payload
                        if (mPayload[iv->id].retransmits < mMaxRetrans) {
                            mPayload[iv->id].retransmits++;
                            DPRINTLN(DBG_WARN, F("CRC Error: Request Complete Retransmit"));
                            mPayload[iv->id].txCmd = iv->getQueuedCmd();
                            DPRINT_IVID(DBG_INFO, iv->id);
                            DBGPRINT(F("prepareDevInformCmd 0x"));
                            DBGHEXLN(mPayload[iv->id].txCmd);
                            mRadio->prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex, true);
                        }
                    } else {  // payload complete
                        DPRINT(DBG_INFO, F("procPyld: cmd:  0x"));
                        DBGHEXLN(mPayload[iv->id].txCmd);
                        DPRINT(DBG_INFO, F("procPyld: txid: 0x"));
                        DBGHEXLN(mPayload[iv->id].txId);
                        DPRINT(DBG_DEBUG, F("procPyld: max:  "));
                        DPRINTLN(DBG_DEBUG, String(mPayload[iv->id].maxPackId));
                        record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                        mPayload[iv->id].complete = true;

                        uint8_t payload[150];
                        uint8_t payloadLen = 0;

                        memset(payload, 0, 150);

                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i++) {
                            if((mPayload[iv->id].len[i] + payloadLen) > 150) {
                                DPRINTLN(DBG_ERROR, F("payload buffer to small!"));
                                break;
                            }
                            memcpy(&payload[payloadLen], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                            payloadLen += (mPayload[iv->id].len[i]);
                            yield();
                        }
                        payloadLen -= 2;

                        if (mSerialDebug) {
                            DPRINT(DBG_INFO, F("Payload ("));
                            DBGPRINT(String(payloadLen));
                            DBGPRINT(F("): "));
                            ah::dumpBuf(payload, payloadLen);
                        }

                        if (NULL == rec) {
                            DPRINTLN(DBG_ERROR, F("record is NULL!"));
                        } else if ((rec->pyldLen == payloadLen) || (0 == rec->pyldLen)) {
                            if (mPayload[iv->id].txId == (TX_REQ_INFO + ALL_FRAMES))
                                mStat->rxSuccess++;

                            rec->ts = mPayload[iv->id].ts;
                            for (uint8_t i = 0; i < rec->length; i++) {
                                iv->addValue(i, payload, rec);
                                yield();
                            }
                            iv->doCalculations();
                            notify(mPayload[iv->id].txCmd, iv);

                            if(AlarmData == mPayload[iv->id].txCmd) {
                                uint8_t i = 0;
                                while(1) {
                                    if(0 == iv->parseAlarmLog(i++, payload, payloadLen))
                                        break;
                                    if (NULL != mCbAlarm)
                                        (mCbAlarm)(iv);
                                    yield();
                                }
                            }
                        } else {
                            DPRINT(DBG_ERROR, F("plausibility check failed, expected "));
                            DBGPRINT(String(rec->pyldLen));
                            DBGPRINTLN(F(" bytes"));
                            mStat->rxFail++;
                        }

                        iv->setQueuedCmdFinished();
                    }
                }
                yield();
            }
        }

    private:
        void notify(uint8_t val, Inverter<> *iv) {
            if(NULL != mCbPayload)
                (mCbPayload)(val, iv);
        }

        bool build(uint8_t id, bool *complete) {
            DPRINTLN(DBG_VERBOSE, F("build"));
            uint16_t crc = 0xffff, crcRcv = 0x0000;
            if (mPayload[id].maxPackId > MAX_PAYLOAD_ENTRIES)
                mPayload[id].maxPackId = MAX_PAYLOAD_ENTRIES;

            // check if all fragments are there
            *complete = true;
            for (uint8_t i = 0; i < mPayload[id].maxPackId; i++) {
                if(mPayload[id].len[i] == 0)
                    *complete = false;
            }
            if(!*complete)
                return false;

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

        void reset(uint8_t id) {
            DPRINT_IVID(DBG_INFO, id);
            DBGPRINTLN(F("resetPayload"));
            memset(mPayload[id].len, 0, MAX_PAYLOAD_ENTRIES);
            mPayload[id].txCmd       = 0;
            mPayload[id].gotFragment = false;
            mPayload[id].retransmits = 0;
            mPayload[id].maxPackId   = MAX_PAYLOAD_ENTRIES;
            mPayload[id].lastFound   = false;
            mPayload[id].complete    = false;
            mPayload[id].requested   = false;
            mPayload[id].ts          = *mTimestamp;
        }

        IApp *mApp;
        HMSYSTEM *mSys;
        HMRADIO *mRadio;
        statistics_t *mStat;
        uint8_t mMaxRetrans;
        uint32_t *mTimestamp;
        invPayload_t mPayload[MAX_NUM_INVERTERS];
        bool mSerialDebug;
        Inverter<> *mHighPrioIv;

        alarmListenerType mCbAlarm;
        payloadListenerType mCbPayload;
};

#endif /*__HM_PAYLOAD_H__*/
