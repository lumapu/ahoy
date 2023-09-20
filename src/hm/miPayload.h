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
    bool limitrequested;
    uint8_t txCmd;
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool complete;
    bool dataAB[3];
    bool stsAB[3];
    uint16_t sts[6];
    uint8_t txId;
    uint8_t invId;
    uint8_t retransmits;
    bool gotFragment;
    uint8_t rtrRes; // for limiting resets
    uint8_t multi_parts;  // for quality
    bool rxTmo;
} miPayload_t;


typedef std::function<void(uint8_t, Inverter<> *)> miPayloadListenerType;


template<class HMSYSTEM, class HMRADIO>
class MiPayload {
    public:
        MiPayload() {}

        void setup(IApp *app, HMSYSTEM *sys, HMRADIO *radio, statistics_t *stat, uint8_t maxRetransmits, uint32_t *timestamp) {
            mApp        = app;
            mSys        = sys;
            mRadio      = radio;
            mStat       = stat;
            mMaxRetrans = maxRetransmits;
            mTimestamp  = timestamp;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                reset(i, false, true);
                mPayload[i].limitrequested = true;
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
            mCbAlarm = cb;
        }

        void loop() {
            if (NULL != mHighPrioIv) {
                ivSend(mHighPrioIv, true); // for e.g. devcontrol commands
                mHighPrioIv = NULL;
            }
        }

        void ivSendHighPrio(Inverter<> *iv) {
            mHighPrioIv = iv;
        }

        void ivSend(Inverter<> *iv, bool highPrio = false) {
            if(!highPrio) {
                if (mPayload[iv->id].requested) {
                    if (!mPayload[iv->id].complete)
                        process(false); // no retransmit

                    if (!mPayload[iv->id].complete && mPayload[iv->id].rxTmo) {
                        if (mSerialDebug)
                            DPRINT_IVID(DBG_INFO, iv->id);
                        if (!mPayload[iv->id].gotFragment) {
                            mStat->rxFailNoAnser++; // got nothing
                            if (mSerialDebug)
                                DBGPRINTLN(F("enqueued cmd failed/timeout"));
                        } else {
                            mStat->rxFail++;        // got "fragments" (part of the required messages)
                                                    // but no complete set of responses
                            if (mSerialDebug) {
                                DBGPRINT(F("no complete Payload received! (retransmits: "));
                                DBGPRINT(String(mPayload[iv->id].retransmits));
                                DBGPRINTLN(F(")"));
                            }
                        }
                        mPayload[iv->id].rxTmo    = true;
                        mPayload[iv->id].complete = true;
                        iv->setQueuedCmdFinished(); // command failed
                    }
                }
            }

            reset(iv->id, !iv->isConnected);
            mPayload[iv->id].requested = true;

            yield();
            if (mSerialDebug){
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
                mRadio->sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, false, false, iv->type == INV_TYPE_4CH);
                mPayload[iv->id].txCmd = iv->devControlCmd;
                mPayload[iv->id].limitrequested = true;

                iv->clearCmdQueue();
            } else {
                uint8_t cmd = iv->getQueuedCmd();
                uint8_t cmd2 = cmd;
                if ( cmd == SystemConfigPara ) { //0x05 for HM-types
                    if (!mPayload[iv->id].limitrequested) { // only do once at startup
                        iv->setQueuedCmdFinished();
                        cmd = iv->getQueuedCmd();
                    } else {
                        mPayload[iv->id].limitrequested = false;
                    }
                }

                if (cmd == 0x01 || cmd == SystemConfigPara ) { //0x1 and 0x05 for HM-types
                    cmd2 = cmd == SystemConfigPara ? 0x01 : 0x00;  //perhaps we can only try to get second frame?
                    cmd  = 0x0f;                              // for MI, these seem to make part of polling the device software and hardware version number command
                }
                if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F("prepareDevInformCmd 0x"));
                    DBGHEXLN(cmd);
                }
                mRadio->sendCmdPacket(iv->radioId.u64, cmd, cmd2, false, false);

                mPayload[iv->id].txCmd = cmd;
                if (iv->type == INV_TYPE_1CH || iv->type == INV_TYPE_2CH) {
                    mPayload[iv->id].dataAB[CH1] = false;
                    mPayload[iv->id].stsAB[CH1] = false;
                    mPayload[iv->id].dataAB[CH0] = false;
                    mPayload[iv->id].stsAB[CH0] = false;
                    if (iv->type == INV_TYPE_2CH) {
                        mPayload[iv->id].dataAB[CH2] = false;
                        mPayload[iv->id].stsAB[CH2] = false;
                    }
                }
            }
        }

        void add(Inverter<> *iv, packet_t *p) {
            //DPRINTLN(DBG_INFO, F("MI got data [0]=") + String(p->packet[0], HEX));
            if (p->packet[0] == (0x08 + ALL_FRAMES)) { // 0x88; MI status response to 0x09
                miStsDecode(iv, p);
            }

            else if (p->packet[0] == (0x11 + SINGLE_FRAME)) { // 0x92; MI status response to 0x11
                miStsDecode(iv, p, CH2);
            }

            else if ( p->packet[0] == 0x09 + ALL_FRAMES ||
                        p->packet[0] == 0x11 + ALL_FRAMES ||
                        ( p->packet[0] >= (0x36 + ALL_FRAMES) && p->packet[0] < (0x39 + SINGLE_FRAME)
                          && mPayload[iv->id].txCmd != 0x0f) ) { // small MI or MI 1500 data responses to 0x09, 0x11, 0x36, 0x37, 0x38 and 0x39
                mPayload[iv->id].txId = p->packet[0];
                miDataDecode(iv,p);
            }

            else if (p->packet[0] == ( 0x0f + ALL_FRAMES)) {
                // MI response from get hardware information request
                miHwDecode(iv, p);
                mPayload[iv->id].txId = p->packet[0];

            } else if ( p->packet[0] == (TX_REQ_INFO + ALL_FRAMES) // response from get information command
                     || (p->packet[0] == 0xB6 && mPayload[iv->id].txCmd != 0x36)) {                   // strange short response from MI-1500 3rd gen; might be misleading!
                // atm, we just do nothing else than print out what we got...
                // for decoding see xls- Data collection instructions - #147ff
                //mPayload[iv->id].txId = p->packet[0];
                DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                uint8_t *pid = &p->packet[9];
                if (*pid == 0x00) {
                    DPRINT(DBG_DEBUG, F("fragment number zero received"));
                    iv->setQueuedCmdFinished();
                } else if (p->packet[9] == 0x81) { // might need some additional check, as this is only meant for short answers!
                    DPRINT_IVID(DBG_WARN, iv->id);
                    DBGPRINTLN(F("seems to use 3rd gen. protocol - switching ivGen!"));
                    iv->ivGen = IV_HM;
                    iv->setQueuedCmdFinished();
                    iv->clearCmdQueue();
                }
            } else if (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES )       // response from dev control command
                    || p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES -1)) {  // response from DRED instruction
                DPRINT_IVID(DBG_DEBUG, iv->id);
                DBGPRINTLN(F("Response from devcontrol request received"));

                mPayload[iv->id].txId = p->packet[0];
                iv->clearDevControlRequest();

                if ((p->packet[9] == 0x5a) && (p->packet[10] == 0x5a)) {
                    mApp->setMqttPowerLimitAck(iv);
                    iv->powerLimitAck = true;
                    if (mSerialDebug) {
                        DPRINT_IVID(DBG_INFO, iv->id);
                        DBGPRINT(F("has accepted power limit set point "));
                        DBGPRINT(String(iv->powerLimit[0]));
                        DBGPRINT(F(" with PowerLimitControl "));
                        DBGPRINTLN(String(iv->powerLimit[1]));
                    }
                    iv->clearCmdQueue();
                    iv->enqueCommand<InfoCommand>(SystemConfigPara); // read back power limit
                }
                iv->devControlCmd = Init;
            } else {  // some other response; copied from hmPayload:process; might not be correct to do that here!!!
                if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO,iv->id);
                    DBGPRINT(F("procPyld: cmd:  0x"));
                    DBGHEXLN(mPayload[iv->id].txCmd);
                    DPRINT_IVID(DBG_INFO,iv->id);
                    DBGPRINT(F("procPyld: txid: 0x"));
                    DBGHEXLN(mPayload[iv->id].txId);
                }

                record_t<> *rec = iv->getRecordStruct(mPayload[iv->id].txCmd);  // choose the parser
                mPayload[iv->id].complete = true;

                uint8_t payload[128];
                uint8_t payloadLen = 0;

                memset(payload, 0, 128);

                payloadLen -= 2;

                if (mSerialDebug) {
                    DPRINT(DBG_INFO, F("Payload ("));
                    DBGPRINT(String(payloadLen));
                    DBGPRINT("): ");
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

                if (IV_MI != iv->ivGen) // only process MI inverters
                    continue; // skip to next inverter

                if ( !mPayload[iv->id].complete &&
                    (mPayload[iv->id].txId != (TX_REQ_INFO + ALL_FRAMES)) &&
                    (mPayload[iv->id].txId <  (0x36 + ALL_FRAMES)) &&
                    (mPayload[iv->id].txId >  (0x39 + ALL_FRAMES)) &&
                    (mPayload[iv->id].txId != (0x09 + ALL_FRAMES)) &&
                    (mPayload[iv->id].txId != (0x11 + ALL_FRAMES)) &&
                    (mPayload[iv->id].txId != (0x88)) &&
                    (mPayload[iv->id].txId != (0x92)) &&
                    (mPayload[iv->id].txId != 0 &&
                    mPayload[iv->id].txCmd != 0x0f)) {
                    // no processing needed if txId is not one of 0x95, 0x88, 0x89, 0x91, 0x92 or response to 0x36ff
                    mPayload[iv->id].complete = true;
                    mPayload[iv->id].rxTmo = true;
                    continue; // skip to next inverter
                }

                if (!mPayload[iv->id].complete) {
                    bool gotAllMsgParts, pyldComplete, fastNext;
                    gotAllMsgParts = build(iv, &pyldComplete, &fastNext);
                    if (!gotAllMsgParts && !pyldComplete) { // payload not complete
                        if ((mPayload[iv->id].requested) && (retransmit)) {
                            if (iv->devControlCmd == Restart || iv->devControlCmd == CleanState_LockAndAlarm) {
                                // This is required to prevent retransmissions without answer.
                                DPRINT_IVID(DBG_INFO, iv->id);
                                DBGPRINTLN(F("Prevent retransmit on Restart / CleanState_LockAndAlarm..."));
                                mPayload[iv->id].retransmits = mMaxRetrans;
                                mPayload[iv->id].rxTmo = true;
                            } else if(iv->devControlCmd == ActivePowerContr) {
                                DPRINT_IVID(DBG_INFO, iv->id);
                                DBGPRINTLN(F("retransmit power limit"));
                                mRadio->sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, true, false);
                            } else {
                                uint8_t cmd = mPayload[iv->id].txCmd;
                                if (mPayload[iv->id].retransmits < mMaxRetrans) {
                                    mPayload[iv->id].retransmits++;
                                    if( !mPayload[iv->id].gotFragment && mPayload[iv->id].rxTmo ) {
                                        DPRINT_IVID(DBG_INFO, iv->id);
                                        DBGPRINTLN(F("nothing received"));
                                        mPayload[iv->id].retransmits = mMaxRetrans;
                                    } else if( !mPayload[iv->id].gotFragment && !mPayload[iv->id].rxTmo ) {
                                        DPRINT_IVID(DBG_INFO, iv->id);
                                        DBGPRINTLN(F("retransmit on failed first request"));
                                        mPayload[iv->id].rxTmo = true;
                                        mRadio->sendCmdPacket(iv->radioId.u64, cmd, cmd, true, false);
                                    } else if ( cmd == 0x0f ) {
                                        //hard/firmware request
                                        mRadio->sendCmdPacket(iv->radioId.u64, 0x0f, 0x00, true, false);
                                        mPayload[id].multi_parts = 0;
                                    } else {
                                        bool change = false;
                                        if ( cmd >= 0x36 && cmd < 0x39 ) { // MI-1500 Data command
                                            if (cmd > 0x36 && mPayload[iv->id].retransmits==1) // first request for the upper channels
                                                change = true;
                                        } else if ( cmd == 0x09 ) {//MI single or dual channel device
                                            if ( mPayload[iv->id].dataAB[CH1] && iv->type == INV_TYPE_2CH  ) {
                                                if (!mPayload[iv->id].stsAB[CH1] && mPayload[iv->id].retransmits<2) {}
                                                    //first try to get missing sts for first channel a second time
                                                else if (!mPayload[iv->id].stsAB[CH2] || !mPayload[iv->id].dataAB[CH2] ) {
                                                    cmd = 0x11;
                                                    change = true;
                                                    if (mPayload[iv->id].rtrRes < 3) //only get back to first channel twice
                                                        mPayload[iv->id].retransmits = 0; //reset counter
                                                }
                                            }
                                        } else if ( cmd == 0x11) {
                                            if ( mPayload[iv->id].dataAB[CH2] ) { // data + status ch2 are there?
                                                if (mPayload[iv->id].stsAB[CH2] && (!mPayload[iv->id].stsAB[CH1] || !mPayload[iv->id].dataAB[CH1])) {
                                                    cmd = 0x09;
                                                    change = true;
                                                }
                                            }
                                        }
                                        DPRINT_IVID(DBG_INFO, iv->id);
                                        if (change) {
                                            DBGPRINT(F("next request is"));
                                            mPayload[iv->id].txCmd = cmd;
                                            mPayload[iv->id].rtrRes++;
                                        } else {
                                            DBGPRINT(F("sth."));
                                            DBGPRINT(F(" missing: Request Retransmit"));
                                        }
                                        DBGPRINT(F(" 0x"));
                                        DBGHEXLN(cmd);
                                        mPayload[id].multi_parts = 0;
                                        mRadio->sendCmdPacket(iv->radioId.u64, cmd, cmd, true, false);
                                        yield();
                                    }
                                } else {
                                    mPayload[iv->id].rxTmo = true;
                                }
                            }
                        }
                    } else if(!gotAllMsgParts && pyldComplete) { // crc error on complete Payload
                        if (mPayload[iv->id].retransmits < mMaxRetrans) {
                            mPayload[iv->id].retransmits++;
                            mPayload[iv->id].txCmd = iv->getQueuedCmd();
                            mPayload[id].multi_parts  = 0;
                            if (mSerialDebug) {
                                DPRINT_IVID(DBG_WARN, iv->id);
                                DBGPRINTLN(F("CRC Error: Request Complete Retransmit"));
                                DPRINT_IVID(DBG_INFO, iv->id);
                                DBGPRINT(F("prepareDevInformCmd 0x"));
                                DBGHEXLN(mPayload[iv->id].txCmd);
                            }
                            mRadio->sendCmdPacket(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].txCmd, false, false);
                        } else {
                            mPayload[iv->id].rxTmo = true;
                        }
                    } else {
                        if (!fastNext) {
                            mPayload[iv->id].rxTmo = true;
                        } else {
                            /*iv->setQueuedCmdFinished();
                            uint8_t cmd = iv->getQueuedCmd();
                            if (mSerialDebug) {
                                DPRINT_IVID(DBG_INFO, iv->id);
                                DBGPRINT(F("fast mode "));
                                DBGPRINT(F("prepareDevInformCmd 0x"));
                                DBGHEXLN(cmd);
                            }
                            mStat->rxSuccess++;
                            //mRadio->prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false);
                            mRadio->prepareDevInformCmd(iv->radioId.u64, iv->getType(),
                                iv->getNextTxChanIndex(), cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false);
                            mPayload[iv->id].txCmd = cmd; */
                            if (mHighPrioIv == NULL)
                                mHighPrioIv = iv;
                        }
                    }
                } else {
                    mPayload[iv->id].rxTmo = true;
                }
                yield();
            }
        }

    private:
        void notify(uint8_t val, Inverter<> *iv) {
            if(NULL != mCbMiPayload)
                (mCbMiPayload)(val, iv);
        }

        void miStsDecode(Inverter<> *iv, packet_t *p, uint8_t stschan = CH1) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);  // choose the record structure
            rec->ts = mPayload[iv->id].ts;
            mPayload[iv->id].gotFragment = true;
            mPayload[iv->id].multi_parts += 3;
            mPayload[iv->id].txId = p->packet[0];
            miStsConsolidate(iv, stschan, rec, p->packet[10], p->packet[12], p->packet[9], p->packet[11]);
            mPayload[iv->id].stsAB[stschan] = true;
            if (mPayload[iv->id].stsAB[CH1] && mPayload[iv->id].stsAB[CH2])
                mPayload[iv->id].stsAB[CH0] = true;
        }

        void miStsConsolidate(Inverter<> *iv, uint8_t stschan,  record_t<> *rec, uint8_t uState, uint8_t uEnum, uint8_t lState = 0, uint8_t lEnum = 0) {
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
            if ( statusMi != mPayload[iv->id].sts[stschan] ) { //sth.'s changed?
                iv->alarmCnt = 1; // minimum...
                if (iv->type != INV_TYPE_1CH && ( statusMi != 3 //sth is or was wrong!
                                                || mPayload[iv->id].sts[stschan] && statusMi == 3 && mPayload[iv->id].sts[stschan] != 3)
                   ) {
                    iv->lastAlarm[stschan] = alarm_t(prntsts, mPayload[iv->id].ts,mPayload[iv->id].ts);
                    iv->alarmCnt = iv->type == INV_TYPE_2CH ? 3 : 5;
                    iv->alarmLastId = iv->alarmMesIndex;
                }

                mPayload[iv->id].sts[stschan] = statusMi;
                if (mSerialDebug) {
                    DPRINT(DBG_WARN, F("New state on CH"));
                    DBGPRINT(String(stschan)); DBGPRINT(F(" ("));
                    DBGPRINT(String(prntsts)); DBGPRINT(F("): "));
                    DBGPRINTLN(iv->getAlarmStr(prntsts));
                }
            }

            if ( !mPayload[iv->id].sts[0] || prntsts < mPayload[iv->id].sts[0] ) {
                mPayload[iv->id].sts[0] = prntsts;
                iv->setValue(iv->getPosByChFld(0, FLD_EVT, rec), rec, prntsts);
            }

            if (iv->alarmMesIndex < rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]) {
                iv->alarmMesIndex = rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]; // seems there's no status per channel in 3rd gen. models?!?
                if (mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DBGPRINT(F("alarm ID incremented to "));
                    DBGPRINTLN(String(iv->alarmMesIndex));
                }
                iv->lastAlarm[0] = alarm_t(prntsts, mPayload[iv->id].ts, mPayload[iv->id].ts);
            }
            /*if(AlarmData == mPayload[iv->id].txCmd) {
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
                            }*/
        }

        void miDataDecode(Inverter<> *iv, packet_t *p) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);  // choose the parser
            rec->ts = mPayload[iv->id].ts;
            mPayload[iv->id].gotFragment = true;
            mPayload[iv->id].multi_parts += 4;

            uint8_t datachan = ( p->packet[0] == 0x89 || p->packet[0] == (0x36 + ALL_FRAMES) ) ? CH1 :
                           ( p->packet[0] == 0x91 || p->packet[0] == (0x37 + ALL_FRAMES) ) ? CH2 :
                           p->packet[0] == (0x38 + ALL_FRAMES) ? CH3 :
                           CH4;
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

            if ( datachan < 3 ) {
                mPayload[iv->id].dataAB[datachan] = true;
            }
            if ( !mPayload[iv->id].dataAB[CH0] && mPayload[iv->id].dataAB[CH1] && mPayload[iv->id].dataAB[CH2] ) {
                mPayload[iv->id].dataAB[CH0] = true;
            }

            if (p->packet[0] >= (0x36 + ALL_FRAMES) ) {
                /*For MI1500:
                if (MI1500) {
                  STAT = (uint8_t)(p->packet[25] );
                  FCNT = (uint8_t)(p->packet[26]);
                  FCODE = (uint8_t)(p->packet[27]);
                }*/
                miStsConsolidate(iv, datachan, rec, p->packet[23], p->packet[24]);

                if (p->packet[0] < (0x39 + ALL_FRAMES) ) {
                    mPayload[iv->id].txCmd++;
                    mPayload[iv->id].retransmits = 0; // reserve retransmissions for each response
                    mPayload[iv->id].complete = false;
                } else {
                    miComplete(iv);
                }
            }

        }

        void miComplete(Inverter<> *iv) {
            if ( mPayload[iv->id].complete )
                return; //if we got second message as well in repreated attempt
            mPayload[iv->id].complete = true;
            DPRINT_IVID(DBG_INFO, iv->id);
            DBGPRINTLN(F("got all msgs"));
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, calcYieldDayCh0(iv,0));

            //preliminary AC calculation...
            float ac_pow = 0;
            for(uint8_t i = 1; i <= iv->channels; i++) {
                if (mPayload[iv->id].sts[i] == 3) {
                    uint8_t pos = iv->getPosByChFld(i, FLD_PDC, rec);
                    ac_pow += iv->getValue(pos, rec);
                }
            }
            ac_pow = (int) (ac_pow*9.5);
            iv->setValue(iv->getPosByChFld(0, FLD_PAC, rec), rec, (float) ac_pow/10);

            iv->doCalculations();
             // update status state-machine,
            iv->isProducing();

            iv->setQueuedCmdFinished();
            mStat->rxSuccess++;
            yield();
            notify(RealTimeRunData_Debug, iv);
        }

        bool build(Inverter<> *iv, bool *complete, bool *fastNext ) {
            DPRINTLN(DBG_VERBOSE, F("build"));
            // check if all messages are there

            *complete = mPayload[iv->id].complete;
            *fastNext = false;
            uint8_t txCmd = mPayload[iv->id].txCmd;

            if(!*complete) {
                DPRINTLN(DBG_VERBOSE, F("incomlete, txCmd is 0x") + String(txCmd, HEX));
                //we got some delayed status msgs?!?
                if ((txCmd == 0x09) || (txCmd == 0x11)) {
                    if (mPayload[iv->id].stsAB[CH0] && mPayload[iv->id].dataAB[CH0]) {
                      miComplete(iv);
                      return true;
                    }
                    return false;
                }
                if (txCmd >= 0x36 && txCmd <= 0x39) {
                    return false;
                }
                if (txCmd == 0x0f) {  //hw info request, at least hw part nr. and version have to be there...
                    bool gotRelevant = iv->getFwVersion()
                        && iv->getChannelFieldValue(CH0, FLD_PART_NUM, iv->getRecordStruct(InverterDevInform_Simple));
                    if (gotRelevant)
                        *fastNext = true;
                    return gotRelevant;
                }
            }

            //check if we want the next request to be executed faster
            if (mPayload[iv->id].gotFragment && txCmd == 0x0f)
                *fastNext = true;
            return true;
        }

        void miHwDecode(Inverter<> *iv, packet_t *p ) {
            record_t<> *rec = iv->getRecordStruct(InverterDevInform_All);  // choose the record structure
            rec->ts = mPayload[iv->id].ts;
            mPayload[iv->id].gotFragment = true;

/*
Polling the device software and hardware version number command
start byte	Command word	 routing address				 target address				 User data	 check	 end byte
byte[0]	 byte[1]	 byte[2]	 byte[3]	 byte[4]	 byte[5]	 byte[6]	 byte[7]	 byte[8]	 byte[9]	 byte[10]	 byte[11]	 byte[12]
0x7e	 0x0f	 xx	 xx	 xx	 xx	 YY	 YY	 YY	 YY	 0x00	 CRC	 0x7f
Command Receipt - First Frame
start byte	Command word	 target address				 routing address				 Multi-frame marking	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 check	 end byte
byte[0]	 byte[1]	 byte[2]	 byte[3]	 byte[4]	 byte[5]	 byte[6]	 byte[7]	 byte[8]	 byte[9]	 byte[10]	 byte[11]	 byte[12]	 byte[13]	 byte[14]	 byte[15]	 byte[16]	 byte[17]	 byte[18]	 byte[19]	 byte[20]	 byte[21]	 byte[22]	 byte[23]	 byte[24]	 byte[25]	 byte[26]	 byte[27]	 byte[28]
0x7e	 0x8f	 YY	 YY	 YY	 YY	 xx	 xx	 xx	 xx	 0x00	 USFWBuild_VER		 APPFWBuild_VER		 APPFWBuild_YYYY		 APPFWBuild_MMDD		 APPFWBuild_HHMM		 APPFW_PN				 HW_VER		 CRC	 0x7f
Command Receipt - Second Frame
start byte	Command word	 target address				 routing address				 Multi-frame marking	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 check	 end byte
byte[0]	 byte[1]	 byte[2]	 byte[3]	 byte[4]	 byte[5]	 byte[6]	 byte[7]	 byte[8]	 byte[9]	 byte[10]	 byte[11]	 byte[12]	 byte[13]	 byte[14]	 byte[15]	 byte[16]	 byte[17]	 byte[18]	 byte[19]	 byte[20]	 byte[21]	 byte[22]	 byte[23]	 byte[24]	 byte[25]	 byte[26]	 byte[27]	 byte[28]
0x7e	 0x8f	 YY	 YY	 YY	 YY	 xx	 xx	 xx	 xx	 0x01	 HW_PN				 HW_FB_TLmValue		 HW_FB_ReSPRT		 HW_GridSamp_ResValule		 HW_ECapValue		 Matching_APPFW_PN				 CRC	 0x7f
Command receipt - third frame
start byte	Command word	 target address				 routing address				 Multi-frame marking	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 User data	 check	 end byte
byte[0]	 byte[1]	 byte[2]	 byte[3]	 byte[4]	 byte[5]	 byte[6]	 byte[7]	 byte[8]	 byte[9]	 byte[10]	 byte[11]	 byte[12]	 byte[13]	 byte[14]	 byte[15]	 byte[16]	 byte[15]	 byte[16]	 byte[17]	 byte[18]
0x7e	 0x8f	 YY	 YY	 YY	 YY	 xx	 xx	 xx	 xx	 0x12	 APPFW_MINVER		 HWInfoAddr		 PNInfoCRC_gusv		 PNInfoCRC_gusv		 CRC	 0x7f
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
                    iv->setValue(i, rec, (float) ((p->packet[(12+2*i)] << 8) + p->packet[(13+2*i)])/1);
                }
                iv->isConnected = true;
                if(mSerialDebug) {
                    DPRINT_IVID(DBG_INFO, iv->id);
                    DPRINT(DBG_INFO,F("HW_VER is "));
                    DBGPRINTLN(String((p->packet[24] << 8) + p->packet[25]));
                }
                record_t<> *rec = iv->getRecordStruct(InverterDevInform_Simple);  // choose the record structure
                rec->ts = mPayload[iv->id].ts;
                iv->setValue(1, rec, (uint32_t) ((p->packet[24] << 8) + p->packet[25])/1);
                mPayload[iv->id].multi_parts +=4;
            } else if ( p->packet[9] == 0x01 || p->packet[9] == 0x10 ) {//second frame for MI, 3rd gen. answers in 0x10
                DPRINT_IVID(DBG_INFO, iv->id);
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
                    record_t<> *rec = iv->getRecordStruct(InverterDevInform_Simple);  // choose the record structure
                    rec->ts = mPayload[iv->id].ts;
                    iv->setValue(0, rec, (uint32_t) ((((p->packet[10] << 8) | p->packet[11]) << 8 | p->packet[12]) << 8 | p->packet[13])/1);

                    if(mSerialDebug) {
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
                    }
                    //notify(InverterDevInform_Simple, iv);
                    mPayload[iv->id].multi_parts +=2;
                    notify(InverterDevInform_All, iv);
                } else {
                    DBGPRINTLN(F("3rd gen. inverter!"));
                }

            } else if ( p->packet[9] == 0x12 ) {//3rd frame
                DPRINT_IVID(DBG_INFO, iv->id);
                DBGPRINTLN(F("got 3rd frame (hw info)"));
                /* according to xlsx (different start byte -1!)
                    byte[11]	 byte[12] APPFW_MINVER
                    byte[13]	 byte[14] HWInfoAddr
                    byte[15]	 byte[16] PNInfoCRC_gusv
                    byte[15]	 byte[16] PNInfoCRC_gusv (this really is double mentionned in xlsx...)
                */
                if(mSerialDebug) {
                    DPRINT(DBG_INFO,F("APPFW_MINVER "));
                    DBGPRINTLN(String((p->packet[10] << 8) + p->packet[11]));
                    DPRINT(DBG_INFO,F("HWInfoAddr "));
                    DBGPRINTLN(String((p->packet[12] << 8) + p->packet[13]));
                    DPRINT(DBG_INFO,F("PNInfoCRC_gusv "));
                    DBGPRINTLN(String((p->packet[14] << 8) + p->packet[15]));
                }
                mPayload[iv->id].multi_parts++;
            }
            if (mPayload[iv->id].multi_parts > 5) {
                iv->setQueuedCmdFinished();
                mPayload[iv->id].complete = true;
                mPayload[iv->id].rxTmo    = true;
                mPayload[iv->id].requested= false;
                mStat->rxSuccess++;
            }
        }

        void reset(uint8_t id, bool setTxTmo = true, bool clrSts = false) {
            memset(mPayload[id].len, 0, MAX_PAYLOAD_ENTRIES);
            mPayload[id].gotFragment = false;
            mPayload[id].rxTmo       = setTxTmo;// design: don't start with complete retransmit
            mPayload[id].rtrRes      = 0;
            mPayload[id].multi_parts = 0;
            mPayload[id].retransmits = 0;
            mPayload[id].complete    = false;
            mPayload[id].dataAB[CH0] = true; //required for 1CH and 2CH devices
            mPayload[id].dataAB[CH1] = true; //required for 1CH and 2CH devices
            mPayload[id].dataAB[CH2] = true; //only required for 2CH devices
            mPayload[id].stsAB[CH0]  = true; //required for 1CH and 2CH devices
            mPayload[id].stsAB[CH1]  = true; //required for 1CH and 2CH devices
            mPayload[id].stsAB[CH2]  = true; //only required for 2CH devices
            mPayload[id].txCmd       = 0;
            mPayload[id].requested   = false;
            mPayload[id].ts          = *mTimestamp;
            mPayload[id].sts[0]      = 0;
            if (clrSts) {                    // only clear channel states at startup
                mPayload[id].sts[CH1]    = 0;
                mPayload[id].sts[CH2]    = 0;
                mPayload[id].sts[CH3]    = 0;
                mPayload[id].sts[CH4]    = 0;
                mPayload[id].sts[5]      = 0; //remember last summarized state
            }
        }

        IApp *mApp;
        HMSYSTEM *mSys;
        HMRADIO *mRadio;
        statistics_t *mStat;
        uint8_t mMaxRetrans;
        uint32_t *mTimestamp;
        miPayload_t mPayload[MAX_NUM_INVERTERS];
        bool mSerialDebug;

        Inverter<> *mHighPrioIv;
        alarmListenerType mCbAlarm;
        payloadListenerType mCbMiPayload;
};

#endif /*__MI_PAYLOAD_H__*/
