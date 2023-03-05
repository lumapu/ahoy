//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __MI_PAYLOAD_H__
#define __MI_PAYLOAD_H__

//#include "hmInverter.h"
#include "../utils/dbg.h"
#include "../utils/crc.h"
#include "../config/config.h"
#include <Arduino.h>

typedef struct {
    uint32_t ts;
    bool requested;
    uint8_t txCmd;
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool complete;
    bool dataAB[2];
    uint8_t sts[5];
    uint8_t txId;
    uint8_t invId;
    uint8_t retransmits;
    /*
    uint8_t data[MAX_PAYLOAD_ENTRIES][MAX_RF_PAYLOAD_SIZE];

    uint8_t maxPackId;
    bool lastFound;
    bool gotFragment;*/
} miPayload_t;


typedef std::function<void(uint8_t)> miPayloadListenerType;


template<class HMSYSTEM>
class MiPayload {
    public:
        MiPayload() {}

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
            mCbMiPayload  = NULL;
        }

        void enableSerialDebug(bool enable) {
            mSerialDebug = enable;
        }

        void addPayloadListener(miPayloadListenerType cb) {
            mCbMiPayload = cb;
        }

        void addAlarmListener(alarmListenerType cb) {
            mCbMiAlarm = cb;
        }

        void loop() {
            /*if(NULL != mHighPrioIv) {
                iv->ivSend(mHighPrioIv, true); // should request firmware version etc.?
                mHighPrioIv = NULL;
            }*/
        }

        void ivSendHighPrio(Inverter<> *iv) {
            mHighPrioIv = iv;
        }

        void ivSend(Inverter<> *iv) {
            reset(iv->id);
            mPayload[iv->id].requested = true;

            yield();
            if (mSerialDebug)
                DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") Requesting Inv SN ") + String(iv->config->serial.u64, HEX));

            //first channel request
            /*uint8_t cmd = iv->type == INV_TYPE_4CH ? 0x36 : 0x09;
            DPRINT(DBG_INFO, F("(#"));
            DBGPRINT(String(iv->id));
            DBGPRINT(F(") prepareDevInformCmd 0x"));
            DBGPRINTLN(String(cmd, HEX));
            mSys->Radio.prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd);
            mPayload[iv->id].txCmd = cmd;*/

            uint8_t cmd = iv->getQueuedCmd();
                DPRINT(DBG_INFO, F("(#"));
                DBGPRINT(String(iv->id));
                DBGPRINT(F(") prepareDevInformCmd 0x"));
                DBGPRINTLN(String(cmd, HEX));
                mSys->Radio.prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd);
                mPayload[iv->id].txCmd = cmd;
            //other channel requests
            /*if (iv->type == INV_TYPE_4CH) {
                for(uint8_t i = 1; i < iv->channels; i++) {
                     iv->enqueCommand<CommandAbstract>(cmd + i, cmd + i);
                    //mSys->Radio.prepareDevInformCmd(iv->radioId.u64, 0x11, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd + i);
                }
            } else */
            if (iv->type == INV_TYPE_2CH) {
                mPayload[iv->id].dataAB[0] = false;
                mPayload[iv->id].dataAB[1] = false;
                //iv->enqueCommand(0x11);
                //mSys->Radio.prepareDevInformCmd(iv->radioId.u64, 0x11, mPayload[iv->id].ts, iv->alarmMesIndex, false, 0x11);
            }
        }

        void add(Inverter<> *iv, packet_t *p) {
            //DPRINTLN(DBG_INFO, F("MI got data [0]=") + String(p->packet[0], HEX));

            if (p->packet[0] == (0x08 + ALL_FRAMES)) { // 0x88; MI status response to 0x09
                //mPayload[iv->id].sts[0] = true;
                miStsDecode(iv, p);
            } else if (p->packet[0] == (0x11 + SINGLE_FRAME)) { // 0x92; MI status response to 0x11
                //mPayload[iv->id].sts[1] = true;
                miStsDecode(iv, p, CH2);
            /*} else if (p->packet[0] == (0x09 + ALL_FRAMES)) { // MI data response to 0x09
                mPayload[iv->id].txId = p->packet[0];
                miDataDecode(iv,p);
                //iv->setQueuedCmdFinished();
                if (INV_TYPE_2CH == iv->type) {
                    //mSys->Radio.prepareDevInformCmd(iv->radioId.u64, iv->getQueuedCmd(), mPayload[iv->id].ts, iv->alarmMesIndex, false, 0x11);
                    //mSys->Radio.prepareDevInformCmd(iv->radioId.u64, 0x11, mPayload[iv->id].ts, iv->alarmMesIndex, false, 0x11);
                } else { // additional check for mPayload[iv->id].stsa == true might be a good idea (request retransmit?)
                    mPayload[iv->id].complete = true;
                    //iv->setQueuedCmdFinished();
                }

            } else if (p->packet[0] == ()) { // MI data response to 0x11
                mPayload[iv->id].txId = p->packet[0];
                miDataDecode(iv,p);
                mStat->rxSuccess++;
                //iv->setQueuedCmdFinished();*/

            } else if ( p->packet[0] == 0x09 + ALL_FRAMES ||
                        p->packet[0] == 0x11 + ALL_FRAMES ||
                        ( p->packet[0] >= (0x36 + ALL_FRAMES) && p->packet[0] < (0x39 + SINGLE_FRAME) ) ) { // small MI or MI 1500 data responses to 0x09, 0x11, 0x36, 0x37, 0x38 and 0x39
                mPayload[iv->id].txId = p->packet[0];
                miDataDecode(iv,p);
                //mStat->rxSuccess++;
                //iv->setQueuedCmdFinished();
                /*if (p->packet[0] < (0x39 + ALL_FRAMES)) {
                    //mSys->Radio.prepareDevInformCmd(iv->radioId.u64, iv->getQueuedCmd(), mPayload[iv->id].ts, iv->alarmMesIndex, false, p->packet[0] + 1 - ALL_FRAMES);
                    //mSys->Radio.prepareDevInformCmd(iv->radioId.u64, p->packet[0] + 1 - ALL_FRAMES, mPayload[iv->id].ts, iv->alarmMesIndex, false, p->packet[0] + 1 - ALL_FRAMES);
                } else {
                    mPayload[iv->id].complete = true;
                    //iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, CALC_YD_CH0);
                    //iv->setQueuedCmdFinished();
                }*/

            //}

            /*if (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES)) {  // response from get information command
                mPayload[iv->id].txId = p->packet[0];
                DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                uint8_t *pid = &p->packet[9];
                if (*pid == 0x00) {
                    DPRINT(DBG_DEBUG, F("fragment number zero received and ignored"));
                } else {
                    DPRINTLN(DBG_DEBUG, "PID: 0x" + String(*pid, HEX));
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
            } */
            } else if (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES)) { // response from dev control command
                DPRINTLN(DBG_DEBUG, F("Response from devcontrol request received"));

                mPayload[iv->id].txId = p->packet[0];
                iv->clearDevControlRequest();

                if ((p->packet[12] == ActivePowerContr) && (p->packet[13] == 0x00)) {
                    String msg = "";
                    if((p->packet[10] == 0x00) && (p->packet[11] == 0x00))
                        mApp->setMqttPowerLimitAck(iv);
                    else
                        msg = "NOT ";
                    DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(" has ") + msg + F("accepted power limit set point ") + String(iv->powerLimit[0]) + F(" with PowerLimitControl ") + String(iv->powerLimit[1]));
                    iv->clearCmdQueue();
                    iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
                }
                iv->devControlCmd = Init;
            } else {  // some other response; copied from hmPayload:process; might not be correct to do that here!!!
                DPRINTLN(DBG_INFO, F("procPyld: cmd:  0x") + String(mPayload[iv->id].txCmd, HEX));
                DPRINTLN(DBG_INFO, F("procPyld: txid: 0x") + String(mPayload[iv->id].txId, HEX));
                //DPRINTLN(DBG_DEBUG, F("procPyld: max:  ") + String(mPayload[iv->id].maxPackId));
                record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                mPayload[iv->id].complete = true;

                uint8_t payload[128];
                uint8_t payloadLen = 0;

                memset(payload, 0, 128);

                /*for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i++) {
                    memcpy(&payload[payloadLen], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                    payloadLen += (mPayload[iv->id].len[i]);
                    yield();
                }*/
                payloadLen -= 2;

                if (mSerialDebug) {
                    DPRINT(DBG_INFO, F("Payload (") + String(payloadLen) + "): ");
                    mSys->Radio.dumpBuf(payload, payloadLen);
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
                    notify(mPayload[iv->id].txCmd);

                    if(AlarmData == mPayload[iv->id].txCmd) {
                        uint8_t i = 0;
                        uint16_t code;
                        uint32_t start, end;
                        while(1) {
                            code = iv->parseAlarmLog(i++, payload, payloadLen, &start, &end);
                            if(0 == code)
                                break;
                            if (NULL != mCbMiAlarm)
                                (mCbMiAlarm)(code, start, end);
                            yield();
                        }
                    }
                } else {
                    DPRINTLN(DBG_ERROR, F("plausibility check failed, expected ") + String(rec->pyldLen) + F(" bytes"));
                    mStat->rxFail++;
                }

                iv->setQueuedCmdFinished();
            }
        }

        void process(bool retransmit) {
            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter

                if (IV_HM == iv->ivGen) // only process MI inverters
                    continue; // skip to next inverter

                /*if ((mPayload[iv->id].txId != (TX_REQ_INFO + ALL_FRAMES)) && (0 != mPayload[iv->id].txId)) {
                    // no processing needed if txId is not 0x95
                    mPayload[iv->id].complete = true;
                    continue; // skip to next inverter
                }*/

                if (!mPayload[iv->id].complete) {
                    bool crcPass, pyldComplete;
                    crcPass = build(iv->id, &pyldComplete);
                    if (!crcPass && !pyldComplete) { // payload not complete
                        if ((mPayload[iv->id].requested) && (retransmit)) {
                            if (iv->devControlCmd == Restart || iv->devControlCmd == CleanState_LockAndAlarm) {
                                // This is required to prevent retransmissions without answer.
                                DPRINTLN(DBG_INFO, F("Prevent retransmit on Restart / CleanState_LockAndAlarm..."));
                                mPayload[iv->id].retransmits = mMaxRetrans;
                            } else if(iv->devControlCmd == ActivePowerContr) {
                                DPRINTLN(DBG_INFO, F("retransmit power limit"));
                                mSys->Radio.sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, true);
                            } else {
                                if (mPayload[iv->id].retransmits < mMaxRetrans) {
                                    mPayload[iv->id].retransmits++;
                                    //mSys->Radio.prepareDevInformCmd(iv->radioId.u64, iv->getQueuedCmd(), mPayload[iv->id].ts, iv->alarmMesIndex, false, 0x11);
                                    DPRINTLN(DBG_WARN, F("missing answer to 0x") + String(iv->getQueuedCmd(), HEX) + F("Request Retransmit"));
                                    mSys->Radio.sendCmdPacket(iv->radioId.u64, iv->getQueuedCmd(), 24, true);
                                    /*if(false == mPayload[iv->id].gotFragment) {
                                        DPRINTLN(DBG_WARN, F("(#") + String(iv->id) + F(") nothing received"));
                                        mPayload[iv->id].retransmits = mMaxRetrans;
                                    } else {
                                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                            if (mPayload[iv->id].len[i] == 0) {
                                                DPRINTLN(DBG_WARN, F("Frame ") + String(i + 1) + F(" missing: Request Retransmit"));
                                                mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME + i), true);
                                                break;  // only request retransmit one frame per loop
                                            }
                                            yield();
                                        }
                                    }*/
                                }
                            }
                        }
                    } else if(!crcPass && pyldComplete) { // crc error on complete Payload
                        if (mPayload[iv->id].retransmits < mMaxRetrans) {
                            mPayload[iv->id].retransmits++;
                            DPRINTLN(DBG_WARN, F("CRC Error: Request Complete Retransmit"));
                            mPayload[iv->id].txCmd = iv->getQueuedCmd();
                            DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") prepareDevInformCmd 0x") + String(mPayload[iv->id].txCmd, HEX));
                            mSys->Radio.prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex, true);
                        }
                    } /*else {  // payload complete
                        DPRINTLN(DBG_INFO, F("procPyld: cmd:  0x") + String(mPayload[iv->id].txCmd, HEX));
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
                            mSys->Radio.dumpBuf(payload, payloadLen);
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
                            DPRINTLN(DBG_ERROR, F("plausibility check failed, expected ") + String(rec->pyldLen) + F(" bytes"));
                            mStat->rxFail++;
                        }

                        iv->setQueuedCmdFinished();
                    }*/
                }
                yield();
            }
        }

    private:
        void notify(uint8_t val) {
            if(NULL != mCbMiPayload)
                (mCbMiPayload)(val);
        }

        void miStsDecode(Inverter<> *iv, packet_t *p, uint8_t chan = CH1) {
            DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(": status msg 0x") + String(p->packet[0], HEX));
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);  // choose the record structure
            rec->ts = mPayload[iv->id].ts;

            uint8_t status  = (p->packet[11] << 8) + p->packet[12];
            uint8_t stschan = p->packet[0] == 0x88 ? CH1 : CH2;
            mPayload[iv->id].dataAB[stschan-1] = true;
            mPayload[iv->id].sts[stschan] = status;
            if ( !mPayload[iv->id].sts[0] || status < mPayload[iv->id].sts[0]) {
                mPayload[iv->id].sts[0] = status;
                iv->setValue(iv->getPosByChFld(0, FLD_EVT, rec), rec, status);
            }

            if ( !mPayload[iv->id].dataAB[0] || !mPayload[iv->id].dataAB[1] ) {
                uint8_t cmd = mPayload[iv->id].dataAB[0] ? 0x11 : 0x09;
                DPRINTLN(DBG_INFO, F("request missing status 0x") + String(cmd, HEX));
                mSys->Radio.prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd);
                mPayload[iv->id].txCmd = cmd;
            }

            if (iv->alarmMesIndex < rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]){
                iv->alarmMesIndex = rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]; // seems there's no status per channel in 3rd gen. models?!?

                DPRINTLN(DBG_INFO, "alarm ID incremented to " + String(iv->alarmMesIndex));
                iv->enqueCommand<InfoCommand>(AlarmData);
            }
        }

        void miDataDecode(Inverter<> *iv, packet_t *p) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);  // choose the parser
            rec->ts = mPayload[iv->id].ts;

            uint8_t datachan = ( p->packet[0] == 0x89 || p->packet[0] == (0x36 + ALL_FRAMES) ) ? CH1 :
                           ( p->packet[0] == 0x91 || p->packet[0] == (0x37 + ALL_FRAMES) ) ? CH2 :
                           p->packet[0] == (0x38 + ALL_FRAMES) ? CH3 :
                           CH4;
            DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(": data msg 0x") + String(p->packet[0], HEX) + F(" channel ") + datachan);
            // count in RF_communication_protocol.xlsx is with offset = -1
            iv->setValue(iv->getPosByChFld(datachan, FLD_UDC, rec), rec, (float)((p->packet[9] << 8) + p->packet[10])/10);
            yield();
            iv->setValue(iv->getPosByChFld(datachan, FLD_IDC, rec), rec, (float)((p->packet[11] << 8) + p->packet[12])/10);
            yield();
            iv->setValue(iv->getPosByChFld(0, FLD_UAC, rec), rec, (float)((p->packet[13] << 8) + p->packet[14])/10);
            yield();
            iv->setValue(iv->getPosByChFld(0, FLD_F, rec), rec, (float) ((p->packet[15] << 8) + p->packet[16])/100);
            iv->setValue(iv->getPosByChFld(datachan, FLD_PDC, rec), rec, (float)((p->packet[17] << 8) + p->packet[18])/10);
            yield();
            iv->setValue(iv->getPosByChFld(datachan, FLD_YD, rec), rec, (float)((p->packet[19] << 8) + p->packet[20])/1);
            yield();
            iv->setValue(iv->getPosByChFld(0, FLD_T, rec), rec, (float) ((int16_t)(p->packet[21] << 8) + p->packet[22])/10);
            iv->setValue(iv->getPosByChFld(0, FLD_IRR, rec), rec, (float) (calcIrradiation(iv, datachan)));
            //AC Power is missing; we may have to calculate, as no respective data is in payload

            if (p->packet[0] >= (0x36 + ALL_FRAMES) ) {

                /*For MI1500:
                if (MI1500) {
                  STAT = (uint8_t)(p->packet[25] );
                  FCNT = (uint8_t)(p->packet[26]);
                  FCODE = (uint8_t)(p->packet[27]);
                }*/

                uint8_t status = (uint8_t)(p->packet[23]);
                mPayload[iv->id].sts[datachan] = status;
                if ( !mPayload[iv->id].sts[0] || status < mPayload[iv->id].sts[0]) {
                    mPayload[iv->id].sts[0] = status;
                    iv->setValue(iv->getPosByChFld(0, FLD_EVT, rec), rec, status);
                }

                if (p->packet[0] < (0x39 + ALL_FRAMES) ) {
                    uint8_t cmd = p->packet[0] - ALL_FRAMES + 1;
                    mSys->Radio.prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd);
                    mPayload[iv->id].txCmd = cmd;
                    mPayload[iv->id].complete = false;
                }

                //iv->setValue(iv->getPosByChFld(0, FLD_EVT, rec), rec, calcMiSts(iv));yield();
                if (iv->alarmMesIndex < rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]){
                    iv->alarmMesIndex = rec->record[iv->getPosByChFld(0, FLD_EVT, rec)];

                    DPRINTLN(DBG_INFO, "alarm ID incremented to " + String(iv->alarmMesIndex));
                    //iv->enqueCommand<InfoCommand>(AlarmData);
                }

            }

            //preliminary AC calculation...
            uint8_t ac_pow = 0;
            //if (mPayload[iv->id].sts[0] == 3) {
                ac_pow = calcPowerDcCh0(iv, 0)*9.5;
            //}
            iv->setValue(iv->getPosByChFld(0, FLD_PAC, rec), rec, (float) (ac_pow/10));

            if ( mPayload[iv->id].sts[0] ) {
                uint8_t cmd = mPayload[iv->id].dataAB[0] ? 0x11 : 0x09;
                if ( mPayload[iv->id].dataAB[0] && mPayload[iv->id].dataAB[1] ) {
                    mPayload[iv->id].complete = true; // For 2 CH devices, this might be too short...
                    DPRINTLN(DBG_INFO, F("complete tree detected"));
                    iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, calcYieldDayCh0(iv,0));
                    iv->doCalculations();
                } else {
                    //retry to get missing status info for one or two channel devices
                    DPRINTLN(DBG_INFO, F("request missing data or status 0x") + String(cmd, HEX));
                    mSys->Radio.prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd);
                    mPayload[iv->id].txCmd = cmd;
                    //iv->enqueCommand(cmd); // mPayload[iv->id].dataAB[1] ? 0x09 : 0x11)
                }
            } // not yet moved to hmInverter::getQueuedCmd
            else if (mPayload[iv->id].txCmd == 0x09 && iv->type == INV_TYPE_2CH) {
                uint8_t cmd = 0x11;
                DPRINTLN(DBG_INFO, F("request second data channel 0x") + String(cmd, HEX));
                mSys->Radio.prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd);
                mPayload[iv->id].txCmd = cmd;
                mPayload[iv->id].complete = false;
            }

            iv->setQueuedCmdFinished();
            mStat->rxSuccess++;
            yield();

            notify(mPayload[iv->id].txCmd);

/*
                            if(AlarmData == mPayload[iv->id].txCmd) {
                                uint8_t i = 0;
                                uint16_t code;
                                uint32_t start, end;
                                while(1) {
                                    code = iv->parseAlarmLog(i++, payload, payloadLen, &start, &end);
                                    if(0 == code)
                                        break;
                                    if (NULL != mCbMiAlarm)
                                        (mCbAlarm)(code, start, end);
                                    yield();
                                }
                            }*/
        }

        bool build(uint8_t id, bool *complete) {
            DPRINTLN(DBG_VERBOSE, F("build"));
            /*uint16_t crc = 0xffff, crcRcv = 0x0000;
            if (mPayload[id].maxPackId > MAX_PAYLOAD_ENTRIES)
                mPayload[id].maxPackId = MAX_PAYLOAD_ENTRIES;
            */
            // check if all messages are there

            *complete = mPayload[id].complete;
            uint8_t txCmd = mPayload[id].txCmd;
            //uint8_t cmd = getQueuedCmd();
            if(!*complete) {
                //if (txCmd == 0x09 || txCmd == 0x11 || txCmd >= 0x36 && txCmd <= 0x39 )
                //    return false;
                DPRINTLN(DBG_VERBOSE, F("incomlete, txCmd is 0x") + String(txCmd, HEX)); // + F("cmd is 0x") + String(cmd, HEX));
            }

            /*for (uint8_t i = 0; i < mPayload[id].maxPackId; i++) {
                if (mPayload[id].len[i] > 0) {
                    if (i == (mPayload[id].maxPackId - 1)) {
                        crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i] - 2, crc);
                        crcRcv = (mPayload[id].data[i][mPayload[id].len[i] - 2] << 8) | (mPayload[id].data[i][mPayload[id].len[i] - 1]);
                    } else
                        crc = ah::crc16(mPayload[id].data[i], mPayload[id].len[i], crc);
                }
                yield();
            }

            return (crc == crcRcv) ? true : false;*/
            return true;
        }

        void reset(uint8_t id) {
            DPRINTLN(DBG_INFO, "resetPayload: id: " + String(id));
            memset(mPayload[id].len, 0, MAX_PAYLOAD_ENTRIES);
            /*
            mPayload[id].gotFragment = false;
            mPayload[id].maxPackId   = MAX_PAYLOAD_ENTRIES;
            mPayload[id].lastFound   = false;*/
            mPayload[id].retransmits = 0;
            mPayload[id].complete    = false;
            mPayload[id].dataAB[0]   = true; //only required for 2CH devices
            mPayload[id].dataAB[1]   = true;
            mPayload[id].txCmd       = 0;
            mPayload[id].requested   = false;
            mPayload[id].ts          = *mTimestamp;
            //mPayload[id].sts[0]      = 0;
            mPayload[id].sts[1]      = 0;
            mPayload[id].sts[2]      = 0;
            mPayload[id].sts[3]      = 0;
            mPayload[id].sts[4]      = 0;
        }


/*        template<class T=uint8_t>
        static T calcMiSts(Inverter<> *iv) {
            if(NULL != iv) {
                T result = 0;
                bool stsComplete = true;
                uint8_t stsCh;
                for(uint8_t i = 1; i <= iv->channels; i++) {
                    stsCh = mPayload[iv->id].sts[i];
                    if (!stsCh) {
                        stsComplete = false;
                    } else if ( !result || stsCh < result ) {
                        result = stsCh;
                        if (stsComplete && stsCh > stsComplete) {
                            stsComplete = stsCh;
                        }
                    }
                }
                mPayload[iv->id].sts[0] = stsComplete;
                return result;
            }
            return 0;
        } */

        IApp *mApp;
        HMSYSTEM *mSys;
        statistics_t *mStat;
        uint8_t mMaxRetrans;
        uint32_t *mTimestamp;
        miPayload_t mPayload[MAX_NUM_INVERTERS];
        bool mSerialDebug;

        Inverter<> *mHighPrioIv;
        alarmListenerType mCbMiAlarm;
        payloadListenerType mCbMiPayload;
};

#endif /*__MI_PAYLOAD_H__*/
