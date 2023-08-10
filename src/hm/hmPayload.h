//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HM_PAYLOAD_H__
#define __HM_PAYLOAD_H__

#include "../utils/dbg.h"
#include "../utils/crc.h"
#include "../config/config.h"
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
    bool rxTmo;
} invPayload_t;


typedef std::function<void(uint8_t)> payloadListenerType;
typedef std::function<void(uint16_t alarmCode, uint32_t start, uint32_t end)> alarmListenerType;


template<class HMSYSTEM>
class HmPayload {
    public:
        HmPayload() {}

        void setup(IApp *app, HMSYSTEM *sys, statistics_t *stat, uint8_t maxRetransmits, uint32_t *timestamp) {
            mApp        = app;
            mSys        = sys;
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

        void zeroYieldDay(Inverter<> *iv) {
            DPRINTLN(DBG_DEBUG, F("zeroYieldDay"));
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            uint8_t pos;
            for(uint8_t ch = 0; ch <= iv->channels; ch++) {
                pos = iv->getPosByChFld(ch, FLD_YD, rec);
                iv->setValue(pos, rec, 0.0f);
                pos = iv->getPosByChFld(ch, FLD_MP, rec);
                iv->setValue(pos, rec, 0.0f);
            }
        }

        void zeroInverterValues(Inverter<> *iv) {
            DPRINTLN(DBG_DEBUG, F("zeroInverterValues"));
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            for(uint8_t ch = 0; ch <= iv->channels; ch++) {
                uint8_t pos = 0;
                for(uint8_t fld = 0; fld < FLD_EVT; fld++) {
                    switch(fld) {
                        case FLD_YD:
                        case FLD_YT:
                        case FLD_MP:
                            continue;
                    }
                    pos = iv->getPosByChFld(ch, fld, rec);
                    iv->setValue(pos, rec, 0.0f);
                }
            }

            notify(RealTimeRunData_Debug);
        }

        void ivSendHighPrio(Inverter<> *iv) {
            mHighPrioIv = iv;
        }

        void ivSend(Inverter<> *iv, bool highPrio = false) {
            bool save_rxTmo;

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

            save_rxTmo = mPayload[iv->id].rxTmo;
            reset(iv->id);
            mPayload[iv->id].rxTmo = save_rxTmo;
            mPayload[iv->id].requested = true;

            yield();
#ifdef undef
            if (mSerialDebug) {
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINT(F("Requesting Inv SN "));
                DBGPRINTLN(String(iv->config->serial.u64, HEX));
            }
#endif

            if (iv->getDevControlRequest()) {
                if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F("Devcontrol request 0x"));
                    DBGPRINT(String(iv->devControlCmd, HEX));
                    DBGPRINT(F(" power limit "));
                    DBGPRINTLN(String(iv->powerLimit[0]));
                }
                mSys->Radio.sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, false);
                mPayload[iv->id].txCmd = iv->devControlCmd;
                //iv->clearCmdQueue();
                //iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
            } else {
                uint8_t cmd = iv->getQueuedCmd();
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINT(F("prepareDevInformCmd 0x"));
                DBGHEXLN(cmd);
                mSys->Radio.prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false);
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
#ifdef AHOY_MQTT_SUPPORT
                        mApp->setMqttPowerLimitAck(iv);
#endif
                    } else {
                        ok = false;
                    }
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F("has "));
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

                if (IV_MI == iv->ivGen) // only process HM inverters
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
                                    mSys->Radio.sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, true);
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
                                            mSys->Radio.prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex, true);
                                        }
                                    } else {
                                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                            if (mPayload[iv->id].len[i] == 0) {
                                                DPRINT_IVID(DBG_WARN, iv->id);
                                                DBGPRINT(F("Frame "));
                                                DBGPRINT(String(i + 1));
                                                DBGPRINTLN(F(" missing: Request Retransmit"));
                                                mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                                break;  // only request retransmit one frame per loop
                                            }
                                            yield();
                                        }
                                    }
                                }
                            } else if (false == mPayload[iv->id].gotFragment) {
                                // only if there is no sign of life
                                mPayload[iv->id].rxTmo = true; // inv might be down, no complete retransmit anymore
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
                            mSys->Radio.prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex, true);
                        } else if (false == mPayload[iv->id].gotFragment) {
                            // only if there is no sign of life
                            mPayload[iv->id].rxTmo = true; // inv might be down, no complete retransmit anymore
                        }
                    } else {  // payload complete
#ifdef undef
                        DPRINT(DBG_INFO, F("procPyld: cmd:  0x"));
                        DBGHEXLN(mPayload[iv->id].txCmd);
                        DPRINT(DBG_INFO, F("procPyld: txid: 0x"));
                        DBGHEXLN(mPayload[iv->id].txId);
#endif
                        DPRINT(DBG_DEBUG, F("procPyld: max:  "));
                        DPRINTLN(DBG_DEBUG, String(mPayload[iv->id].maxPackId));

                        record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                        mPayload[iv->id].complete = true;
                        mPayload[iv->id].rxTmo = false;

                        uint8_t payload[128];
                        uint8_t payloadLen = 0;

                        memset(payload, 0, 128);

                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i++) {
                            memcpy(&payload[payloadLen], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                            payloadLen += (mPayload[iv->id].len[i]);
                            yield();
                        }
                        payloadLen -= 2;

#ifdef undef
                        if (mSerialDebug) {
                            DPRINT(DBG_INFO, F("Payload ("));
                            DBGPRINT(String(payloadLen));
                            DBGPRINT(F("): "));
                            mSys->Radio.dumpBuf(payload, payloadLen);
                        }
#endif

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
                            uint8_t pos = iv->getPosByChFld(CH0, FLD_PAC, rec);
                            if (pos != 0xff) {
                                float ac_power = iv->getValue(pos, rec);
                                mSys->handle_pac (iv, (uint16_t)(ac_power+0.5f));
                            }

                            notify(mPayload[iv->id].txCmd);

                            if(AlarmData == mPayload[iv->id].txCmd) {
                                uint8_t i = 0;
                                uint16_t code;
                                uint32_t start, end;
                                while(1) {
                                    code = iv->parseAlarmLog(i++, payload, payloadLen, &start, &end);
                                    if(0 == code)
                                        break;
                                    if (NULL != mCbAlarm)
                                        (mCbAlarm)(code, start, end);
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
        void notify(uint8_t val) {
            if(NULL != mCbPayload)
                (mCbPayload)(val);
        }

        void notify(uint16_t code, uint32_t start, uint32_t endTime) {
            if (NULL != mCbAlarm)
                (mCbAlarm)(code, start, endTime);
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
#ifdef undef
            DPRINT_IVID(DBG_INFO, id);
            DBGPRINTLN(F("resetPayload"));
#endif
            memset(mPayload[id].len, 0, MAX_PAYLOAD_ENTRIES);
            mPayload[id].txCmd       = 0;
            mPayload[id].gotFragment = false;
            mPayload[id].retransmits = 0;
            mPayload[id].maxPackId   = MAX_PAYLOAD_ENTRIES;
            mPayload[id].lastFound   = false;
            mPayload[id].complete    = false;
            mPayload[id].requested   = false;
            mPayload[id].ts          = *mTimestamp;
            mPayload[id].rxTmo       = true; // design: dont start with complete retransmit
        }

        IApp *mApp;
        HMSYSTEM *mSys;
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
