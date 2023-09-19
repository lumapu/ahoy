//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HMS_PAYLOAD_H__
#define __HMS_PAYLOAD_H__

#include "../utils/dbg.h"
#include "../utils/crc.h"
#include "../config/config.h"
#include <Arduino.h>

#define HMS_TIMEOUT_SEC  30 // 30s * 1000

typedef struct {
    uint8_t txCmd;
    uint8_t txId;
    //uint8_t invId;
    uint32_t ts;
    uint8_t data[MAX_PAYLOAD_ENTRIES][MAX_RF_PAYLOAD_SIZE];
    int8_t rssi[MAX_PAYLOAD_ENTRIES];
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool complete;
    uint8_t maxPackId;
    bool lastFound;
    uint8_t retransmits;
    bool requested;
    bool gotFragment;
} hmsPayload_t;


typedef std::function<void(uint8_t, Inverter<> *)> payloadListenerType;
typedef std::function<void(Inverter<> *)> alarmListenerType;


template<class HMSYSTEM, class RADIO>
class HmsPayload {
    public:
        HmsPayload() {}

        void setup(IApp *app, HMSYSTEM *sys, RADIO *radio, statistics_t *stat, uint8_t maxRetransmits, uint32_t *timestamp) {
            mApp        = app;
            mSys        = sys;
            mRadio      = radio;
            mStat       = stat;
            mMaxRetrans = maxRetransmits;
            mTimestamp  = timestamp;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                reset(i);
                mIvCmd56Cnt[i] = 0;
            }
            mSerialDebug = false;
            mHighPrioIv  = NULL;
            mCbAlarm     = NULL;
            mCbPayload   = NULL;
            //mLastRx      = 0;
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
            if(NULL != mHighPrioIv) {
                ivSend(mHighPrioIv, true);
                mHighPrioIv = NULL;
            }
        }

        void ivSendHighPrio(Inverter<> *iv) {
            mHighPrioIv = iv;
        }

        void ivSend(Inverter<> *iv, bool highPrio = false) {
            if ((IV_HMS != iv->ivGen) && (IV_HMT != iv->ivGen)) // only process HMS inverters
                return;

            if(!highPrio) {
                if (mPayload[iv->id].requested) {
                    if (!mPayload[iv->id].complete)
                        process(false); // no retransmit

                    if (!mPayload[iv->id].complete) {
                        if (MAX_PAYLOAD_ENTRIES == mPayload[iv->id].maxPackId)
                            mStat->rxFailNoAnser++; // got nothing
                        else
                            mStat->rxFail++; // got fragments but not complete response

                        iv->setQueuedCmdFinished();  // command failed
                        if (mSerialDebug)
                            DPRINTLN(DBG_INFO, F("enqueued cmd failed/timeout"));
                        /*if (mSerialDebug) {
                            DPRINT(DBG_INFO, F("(#"));
                            DBGPRINT(String(iv->id));
                            DBGPRINT(F(") no Payload received! (retransmits: "));
                            DBGPRINT(String(mPayload[iv->id].retransmits));
                            DBGPRINTLN(F(")"));
                        }*/
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

            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            if (iv->getDevControlRequest()) {
                if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F("Devcontrol request 0x"));
                    DBGPRINT(String(iv->devControlCmd, HEX));
                    DBGPRINT(F(" power limit "));
                    DBGPRINTLN(String(iv->powerLimit[0]));
                }
                iv->powerLimitAck = false;
                mRadio->sendControlPacket(&iv->radioId.u64, iv->devControlCmd, iv->powerLimit, false);
                mPayload[iv->id].txCmd = iv->devControlCmd;
                //iv->clearCmdQueue();
                //iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
            } else if(((rec->ts + HMS_TIMEOUT_SEC) < *mTimestamp) && (mIvCmd56Cnt[iv->id] < 3)) {
                mRadio->switchFrequency(&iv->radioId.u64, HOY_BOOT_FREQ_KHZ, WORK_FREQ_KHZ);
                mIvCmd56Cnt[iv->id]++;
            } else {
                if(++mIvCmd56Cnt[iv->id] == 10)
                    mIvCmd56Cnt[iv->id] = 0;
                uint8_t cmd = iv->getQueuedCmd();
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINT(F("prepareDevInformCmd 0x"));
                DBGHEXLN(cmd);
                mRadio->prepareDevInformCmd(&iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmLastId, false);
                mPayload[iv->id].txCmd = cmd;
            }
        }

        void add(Inverter<> *iv, hmsPacket_t *p) {
            if (p->data[1] == (TX_REQ_INFO + ALL_FRAMES)) {  // response from get information command
                mPayload[iv->id].txId = p->data[1];
                DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                uint8_t *pid = &p->data[10];
                if (*pid == 0x00) {
                    DPRINT(DBG_DEBUG, F("fragment number zero received and ignored"));
                } else {
                    DPRINTLN(DBG_DEBUG, "PID: 0x" + String(*pid, HEX));
                    if ((*pid & 0x7F) < MAX_PAYLOAD_ENTRIES) {
                        memcpy(mPayload[iv->id].data[(*pid & 0x7F) - 1], &p->data[11], p->data[0] - 11);
                        mPayload[iv->id].len[(*pid & 0x7F) - 1] = p->data[0] -11;
                        mPayload[iv->id].gotFragment = true;
                        mPayload[iv->id].rssi[(*pid & 0x7F) - 1] = p->rssi;
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
            } else if (p->data[1] == (TX_REQ_DEVCONTROL + ALL_FRAMES)) { // response from dev control command
                DPRINTLN(DBG_DEBUG, F("Response from devcontrol request received"));

                mPayload[iv->id].txId = p->data[1];
                iv->clearDevControlRequest();

                if ((p->data[13] == ActivePowerContr) && (p->data[14] == 0x00)) {
                    bool ok = true;
                    if((p->data[11] == 0x00) && (p->data[12] == 0x00)) {
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

                if ((IV_HMS != iv->ivGen) && (IV_HMT != iv->ivGen)) // only process HMS inverters
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
                                    DPRINTLN(DBG_INFO, F("retransmit power limit"));
                                    mRadio->sendControlPacket(&iv->radioId.u64, iv->devControlCmd, iv->powerLimit, true);
                                } else {
                                    if(false == mPayload[iv->id].gotFragment) {

                                        //DPRINTLN(DBG_WARN, F("nothing received: Request Complete Retransmit"));
                                        //mPayload[iv->id].txCmd = iv->getQueuedCmd();
                                        //DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") prepareDevInformCmd 0x") + String(mPayload[iv->id].txCmd, HEX));
                                        //mRadio->prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmLastId, true);

                                        DPRINT_IVID(DBG_INFO, iv->id);
                                        DBGPRINTLN(F("nothing received"));
                                        mPayload[iv->id].retransmits = mMaxRetrans;
                                    } else {
                                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                            if (mPayload[iv->id].len[i] == 0) {
                                                DPRINT(DBG_WARN, F("Frame "));
                                                DBGPRINT(String(i + 1));
                                                DBGPRINTLN(F(" missing: Request Retransmit"));
                                                //mRadio->sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                                break;  // only request retransmit one frame per loop
                                            }
                                            yield();
                                        }
                                    }
                                }
                            }
                        }
                    } /*else if(!crcPass && pyldComplete) { // crc error on complete Payload
                        if (mPayload[iv->id].retransmits < mMaxRetrans) {
                            mPayload[iv->id].retransmits++;
                            DPRINTLN(DBG_WARN, F("CRC Error: Request Complete Retransmit"));
                            mPayload[iv->id].txCmd = iv->getQueuedCmd();
                            DPRINT(DBG_INFO, F("(#"));
                            DBGPRINT(String(iv->id));
                            DBGPRINT(F(") prepareDevInformCmd 0x"));
                            DBGPRINTLN(String(mPayload[iv->id].txCmd, HEX));
                            mRadio->prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmLastId, true);
                        }
                    }*/ else {  // payload complete
                        DPRINT(DBG_INFO, F("procPyld: cmd:  0x"));
                        DBGPRINTLN(String(mPayload[iv->id].txCmd, HEX));
                        //DPRINT(DBG_DEBUG, F("procPyld: txid: 0x"));
                        //DBGPRINTLN(String(mPayload[iv->id].txId, HEX));
                        DPRINTLN(DBG_DEBUG, F("procPyld: max:  ") + String(mPayload[iv->id].maxPackId));
                        record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                        mPayload[iv->id].complete = true;

                        uint8_t payload[150];
                        uint8_t payloadLen = 0;

                        memset(payload, 0, 150);

                        int8_t rssi = -127;

                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i++) {
                            if((mPayload[iv->id].len[i] + payloadLen) > 150) {
                                DPRINTLN(DBG_ERROR, F("payload buffer to small!"));
                                break;
                            }
                            memcpy(&payload[payloadLen], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                            payloadLen += (mPayload[iv->id].len[i]);
                            // get worst RSSI
                            if(mPayload[iv->id].rssi[i] > rssi)
                                rssi = mPayload[iv->id].rssi[i];
                            yield();
                        }
                        payloadLen -= 2;

                        if (mSerialDebug) {
                            DPRINT_IVID(DBG_INFO, iv->id);
                            DBGPRINT(F("Payload ("));
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
                            iv->rssi = rssi;
                            iv->doCalculations();
                            notify(mPayload[iv->id].txCmd, iv);

                            if(AlarmData == mPayload[iv->id].txCmd) {
                                uint8_t i = 0;
                                uint32_t start, end;
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
                        crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i] - 1, crc);
                        crcRcv = (mPayload[id].data[i][mPayload[id].len[i] - 2] << 8) | (mPayload[id].data[i][mPayload[id].len[i] - 1]);
                    } else
                        crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i], crc);
                }
                yield();
            }

            return (crc == crcRcv) ? true : false;
        }

        void reset(uint8_t id) {
            //DPRINT(DBG_INFO, "resetPayload: id: ");
            //DBGPRINTLN(String(id));
            memset(&mPayload[id], 0, sizeof(hmsPayload_t));
            mPayload[id].txCmd       = 0;
            mPayload[id].gotFragment = false;
            //mPayload[id].retransmits = 0;
            mPayload[id].maxPackId   = MAX_PAYLOAD_ENTRIES;
            mPayload[id].lastFound   = false;
            mPayload[id].complete    = false;
            mPayload[id].requested   = false;
            mPayload[id].ts          = *mTimestamp;
        }

        IApp *mApp;
        HMSYSTEM *mSys;
        RADIO *mRadio;
        statistics_t *mStat;
        uint8_t mMaxRetrans;
        uint32_t *mTimestamp;
        //uint32_t mLastRx;
        hmsPayload_t mPayload[MAX_NUM_INVERTERS];
        uint8_t mIvCmd56Cnt[MAX_NUM_INVERTERS];
        bool mSerialDebug;
        Inverter<> *mHighPrioIv;

        alarmListenerType mCbAlarm;
        payloadListenerType mCbPayload;
};

#endif /*__HMS_PAYLOAD_H__*/
