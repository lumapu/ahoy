//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HM_PAYLOAD_H__
#define __HM_PAYLOAD_H__

#include "../utils/dbg.h"
#include "../utils/crc.h"
#include "../config/config.h"
#include "hmRadio.h"
#if defined(ESP32)
#include "../hms/cmt2300a.h"
#endif
#include <Arduino.h>

#define HMS_TIMEOUT_SEC  30

typedef struct {
    uint8_t txCmd;
    uint8_t txId;
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
    bool rxTmo;
    uint32_t sendMillis;
} invPayload_t;


typedef std::function<void(uint8_t, Inverter<> *)> payloadListenerType;
typedef std::function<void(Inverter<> *)> alarmListenerType;


template<class HMSYSTEM>
class HmPayload {
    public:
        HmPayload() {}

        void setup(IApp *app, HMSYSTEM *sys, uint8_t maxRetransmits, uint32_t *timestamp) {
            mApp        = app;
            mSys        = sys;
            mMaxRetrans = maxRetransmits;
            mTimestamp  = timestamp;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                reset(i);
                mIvCmd56Cnt[i] = 0;
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
                ivSend(mHighPrioIv, true); // for e.g. devcontrol commands
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
                            iv->radioStatistics.rxFailNoAnser++; // got nothing
                            if (mSerialDebug)
                                DBGPRINTLN(F("enqueued cmd failed/timeout"));
                        } else {
                            iv->radioStatistics.rxFail++; // got fragments but not complete response
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

            reset(iv->id, !iv->isAvailable());
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
                    DHEX(iv->devControlCmd);
                    DBGPRINT(F(" power limit "));
                    DBGPRINTLN(String(iv->powerLimit[0]));
                }
                iv->powerLimitAck = false;
                iv->radio->sendControlPacket(iv, iv->devControlCmd, iv->powerLimit, false);
                mPayload[iv->id].txCmd = iv->devControlCmd;
                //iv->clearCmdQueue();
                //iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
            } else {
                #if defined(ESP32)
                if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen)) {
                    record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                    if(((rec->ts + HMS_TIMEOUT_SEC) < *mTimestamp) && (mIvCmd56Cnt[iv->id] < 3)) {
                        iv->radio->switchFrequency(iv, HOY_BOOT_FREQ_KHZ, WORK_FREQ_KHZ);
                        mIvCmd56Cnt[iv->id]++;
                        return;
                    } else if(++mIvCmd56Cnt[iv->id] == 10)
                        mIvCmd56Cnt[iv->id] = 0;
                }
                #endif
                uint8_t cmd = iv->getQueuedCmd();
                if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F("prepareDevInformCmd 0x"));
                    DBGHEXLN(cmd);
                }
                iv->radio->prepareDevInformCmd(iv, cmd, mPayload[iv->id].ts, iv->alarmLastId, false);
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

                    if (mSerialDebug) {
                        DPRINT_IVID(DBG_INFO, iv->id);
                        DBGPRINT(F(" has "));
                        if(!ok) DBGPRINT(F("not "));
                        DBGPRINT(F("accepted power limit set point "));
                        DBGPRINT(String(iv->powerLimit[0]));
                        DBGPRINT(F(" with PowerLimitControl "));
                        DBGPRINTLN(String(iv->powerLimit[1]));
                    }

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

                if ((mPayload[iv->id].txId != (TX_REQ_INFO + ALL_FRAMES)) && (0 != mPayload[iv->id].txId)) {
                    // no processing needed if txId is not 0x95
                    mPayload[iv->id].complete = true;
                    continue; // skip to next inverter
                }

                if((IV_HMS == iv->ivGen) || (IV_HMT == iv->ivGen)) {
                    if((mPayload[iv->id].sendMillis + 400) > millis())
                        return; // to fast, wait until packets are received!
                }

                if (!mPayload[iv->id].complete) {
                    bool crcPass, pyldComplete, fastNext;

                    crcPass = build(iv, &pyldComplete, &fastNext);
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
                                    iv->radio->sendControlPacket(iv, iv->devControlCmd, iv->powerLimit, true);
                                } else {
                                    if(false == mPayload[iv->id].gotFragment) {
                                        DPRINT_IVID(DBG_WARN, iv->id);
                                        if (mPayload[iv->id].rxTmo) {
                                            DBGPRINTLN(F("nothing received"));
                                            mPayload[iv->id].retransmits = mMaxRetrans;
                                        } else {
                                            DBGPRINTLN(F("nothing received: complete retransmit"));
                                            mPayload[iv->id].txCmd = iv->getQueuedCmd();
                                            DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") prepareDevInformCmd 0x") + String(mPayload[iv->id].txCmd, HEX));
                                            iv->radio->prepareDevInformCmd(iv, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex, true);
                                        }
                                    } else {
                                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                            if (mPayload[iv->id].len[i] == 0) {
                                                if (mSerialDebug) {
                                                    DPRINT_IVID(DBG_WARN, iv->id);
                                                    DBGPRINT(F("Frame "));
                                                    DBGPRINT(String(i + 1));
                                                    DBGPRINTLN(F(" missing: Request Retransmit"));
                                                }
                                                iv->radio->sendCmdPacket(iv, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                                break;  // only request retransmit one frame per loop
                                            }
                                            yield();
                                        }
                                    }
                                }
                            }
                        } else if (false == mPayload[iv->id].gotFragment) {
                            // only if there is no sign of life
                            mPayload[iv->id].rxTmo = true; // inv might be down, no complete retransmit anymore
                        }
                    } else if(!crcPass && pyldComplete) { // crc error on complete Payload
                        if (mPayload[iv->id].retransmits < mMaxRetrans) {
                            mPayload[iv->id].retransmits++;
                            mPayload[iv->id].txCmd = iv->getQueuedCmd();
                            if (mSerialDebug) {
                                DPRINT_IVID(DBG_WARN, iv->id);
                                DBGPRINTLN(F("CRC Error: Request Complete Retransmit"));
                                DPRINT_IVID(DBG_INFO, iv->id);
                                DBGPRINT(F("prepareDevInformCmd 0x"));
                                DBGHEXLN(mPayload[iv->id].txCmd);
                            }
                            iv->radio->prepareDevInformCmd(iv, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmLastId, true);
                        }
                    } else {  // payload complete
                        if (mSerialDebug) {
                            DPRINT_IVID(DBG_INFO, iv->id);
                            DBGPRINT(F("procPyld: cmd:  0x"));
                            DBGHEXLN(mPayload[iv->id].txCmd);
                            //DPRINT(DBG_DEBUG, F("procPyld: txid: 0x"));
                            //DBGHEXLN(mPayload[iv->id].txId);
                            DPRINT(DBG_DEBUG, F("procPyld: max:  "));
                            DPRINTLN(DBG_DEBUG, String(mPayload[iv->id].maxPackId));
                        }
                        record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                        mPayload[iv->id].complete = true;
                        mPayload[iv->id].requested = false;
                        mPayload[iv->id].rxTmo = false;

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
                                iv->radioStatistics.rxSuccess++;

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
                                while(1) {
                                    if(0 == iv->parseAlarmLog(i++, payload, payloadLen))
                                        break;
                                    if (NULL != mCbAlarm)
                                        (mCbAlarm)(iv);
                                    yield();
                                }
                            }
                            if (fastNext && (mHighPrioIv == NULL)) {
                                /*iv->setQueuedCmdFinished();
                                uint8_t cmd = iv->getQueuedCmd();
                                if (mSerialDebug) {
                                    DPRINT_IVID(DBG_INFO, iv->id);
                                    DBGPRINT(F("fast mode "));
                                    DBGPRINT(F("prepareDevInformCmd 0x"));
                                    DBGHEXLN(cmd);
                                }
                                iv->radioStatistics.rxSuccess++;
                                iv->radio->prepareDevInformCmd(iv, cmd, mPayload[iv->id].ts, iv->alarmLastId, false);
                                mPayload[iv->id].txCmd = cmd;
                                */
                                mHighPrioIv = iv;
                            }

                        } else {
                            if (mSerialDebug) {
                                DPRINT(DBG_ERROR, F("plausibility check failed, expected "));
                                DBGPRINT(String(rec->pyldLen));
                                DBGPRINTLN(F(" bytes"));
                            }
                            iv->radioStatistics.rxFail++;
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

        bool build(Inverter<> *iv, bool *complete, bool *fastNext ) {
            DPRINTLN(DBG_VERBOSE, F("build"));
            uint16_t crc = 0xffff, crcRcv = 0x0000;
            if (mPayload[iv->id].maxPackId > MAX_PAYLOAD_ENTRIES)
                mPayload[iv->id].maxPackId = MAX_PAYLOAD_ENTRIES;

            // check if all fragments are there
            *complete = true;
            *fastNext = false;
            for (uint8_t i = 0; i < mPayload[iv->id].maxPackId; i++) {
                if(mPayload[iv->id].len[i] == 0) {
                    *complete = false;
                }
            }
            if(!*complete)
                return false;

            for (uint8_t i = 0; i < mPayload[iv->id].maxPackId; i++) {
                if (mPayload[iv->id].len[i] > 0) {
                    if (i == (mPayload[iv->id].maxPackId - 1)) {
                        crc = ah::crc16(mPayload[iv->id].data[i], mPayload[iv->id].len[i] - 2, crc);
                        crcRcv = (mPayload[iv->id].data[i][mPayload[iv->id].len[i] - 2] << 8) | (mPayload[iv->id].data[i][mPayload[iv->id].len[i] - 1]);
                    } else
                        crc = ah::crc16(mPayload[iv->id].data[i], mPayload[iv->id].len[i], crc);
                }
                yield();
            }

            //return (crc == crcRcv) ? true : false;
            if (crc != crcRcv)
                return false;

            //requests to cause the next request to be executed immediately
            if (mPayload[iv->id].gotFragment && ((mPayload[iv->id].txCmd < RealTimeRunData_Debug) || (mPayload[iv->id].txCmd >= AlarmData))) {
                *fastNext = true;
            }

            return true;
        }

        void reset(uint8_t id, bool setTxTmo = true) {
            //DPRINT_IVID(DBG_INFO, id);
            //DBGPRINTLN(F("resetPayload"));
            memset(mPayload[id].len, 0, MAX_PAYLOAD_ENTRIES);
            mPayload[id].txCmd       = 0;
            mPayload[id].gotFragment = false;
            mPayload[id].retransmits = 0;
            mPayload[id].maxPackId   = MAX_PAYLOAD_ENTRIES;
            mPayload[id].lastFound   = false;
            mPayload[id].complete    = false;
            mPayload[id].requested   = false;
            mPayload[id].ts          = *mTimestamp;
            mPayload[id].rxTmo       = setTxTmo; // design: don't start with complete retransmit
            mPayload[id].sendMillis  = millis();
        }

        IApp *mApp;
        HMSYSTEM *mSys;
        uint8_t mMaxRetrans;
        uint32_t *mTimestamp;
        invPayload_t mPayload[MAX_NUM_INVERTERS];
        uint8_t mIvCmd56Cnt[MAX_NUM_INVERTERS];
        bool mSerialDebug;
        Inverter<> *mHighPrioIv;

        alarmListenerType mCbAlarm;
        payloadListenerType mCbPayload;
};

#endif /*__HM_PAYLOAD_H__*/
