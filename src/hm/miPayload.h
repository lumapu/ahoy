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
    bool stsa;
    bool stsb;
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

            uint8_t cmd = iv->type == INV_TYPE_4CH ? 0x36 : 0x09; //iv->getQueuedCmd();
            DPRINTLN(DBG_INFO, F("(#") + String(iv->id) + F(") prepareDevInformCmd"));
            mRadio->prepareDevInformCmd(iv->radioId.u64, cmd, mPayload[iv->id].ts, iv->alarmMesIndex, false, cmd);
            mPayload[iv->id].txCmd = cmd;
        }

        void add(Inverter<> *iv, packet_t *p) {
            //DPRINTLN(DBG_INFO, F("MI got data [0]=") + String(p->packet[0], HEX));

            if (p->packet[0] == (0x08 + ALL_FRAMES)) { // 0x88; MI status response to 0x09
                mPayload[iv->id].stsa = true;
                miStsDecode(iv, p);
            } else if (p->packet[0] == (0x11 + SINGLE_FRAME)) { // 0x92; MI status response to 0x11
                mPayload[iv->id].stsb = true;
                miStsDecode(iv, p, CH2);
            } else if (p->packet[0] == (0x09 + ALL_FRAMES)) { // MI data response to 0x09
                mPayload[iv->id].txId = p->packet[0];
                miDataDecode(iv,p);
                iv->setQueuedCmdFinished();
                if (INV_TYPE_2CH == iv->type) {
                    //mRadio->prepareDevInformCmd(iv->radioId.u64, iv->getQueuedCmd(), mPayload[iv->id].ts, iv->alarmMesIndex, false, 0x11);
                    mRadio->prepareDevInformCmd(iv->radioId.u64, 0x11, mPayload[iv->id].ts, iv->alarmMesIndex, false, 0x11);
                } else { // additional check for mPayload[iv->id].stsa == true might be a good idea (request retransmit?)
                    mPayload[iv->id].complete = true;
                    //iv->setQueuedCmdFinished();
                }

            } else if (p->packet[0] == (0x11 + ALL_FRAMES)) { // MI data response to 0x11
                mPayload[iv->id].txId = p->packet[0];
                mPayload[iv->id].complete = true;
                miDataDecode(iv,p);
                iv->setQueuedCmdFinished();

            } else if (p->packet[0] >= (0x36 + ALL_FRAMES) && p->packet[0] < (0x39 + SINGLE_FRAME)) { // MI 1500 data response to 0x36, 0x37, 0x38 and 0x39
                mPayload[iv->id].txId = p->packet[0];
                miDataDecode(iv,p);
                iv->setQueuedCmdFinished();
                if (p->packet[0] < (0x39 + ALL_FRAMES)) {
                    //mRadio->prepareDevInformCmd(iv->radioId.u64, iv->getQueuedCmd(), mPayload[iv->id].ts, iv->alarmMesIndex, false, p->packet[0] + 1 - ALL_FRAMES);
                    mRadio->prepareDevInformCmd(iv->radioId.u64, p->packet[0] + 1 - ALL_FRAMES, mPayload[iv->id].ts, iv->alarmMesIndex, false, p->packet[0] + 1 - ALL_FRAMES);
                } else {
                    mPayload[iv->id].complete = true;
                    //iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, CALC_YD_CH0);
                    //iv->setQueuedCmdFinished();
                }

            /*}

            if (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES)) {  // response from get information command
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

                if (IV_MI != iv->ivGen) // only process MI inverters
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
                                mRadio->sendControlPacket(iv->radioId.u64, iv->devControlCmd, iv->powerLimit, true);
                            } else {
                                if (mPayload[iv->id].retransmits < mMaxRetrans) {
                                    mPayload[iv->id].retransmits++;
                                    //mRadio->prepareDevInformCmd(iv->radioId.u64, iv->getQueuedCmd(), mPayload[iv->id].ts, iv->alarmMesIndex, false, 0x11);
                                    mRadio->sendCmdPacket(iv->radioId.u64, iv->getQueuedCmd(), 24, true);
                                    /*if(false == mPayload[iv->id].gotFragment) {
                                        DPRINTLN(DBG_WARN, F("(#") + String(iv->id) + F(") nothing received"));
                                        mPayload[iv->id].retransmits = mMaxRetrans;
                                    } else {
                                        for (uint8_t i = 0; i < (mPayload[iv->id].maxPackId - 1); i++) {
                                            if (mPayload[iv->id].len[i] == 0) {
                                                DPRINTLN(DBG_WARN, F("Frame ") + String(i + 1) + F(" missing: Request Retransmit"));
                                                mRadio->sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME + i), true);
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
                            mRadio->prepareDevInformCmd(iv->radioId.u64, mPayload[iv->id].txCmd, mPayload[iv->id].ts, iv->alarmMesIndex, true);
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

            //iv->setValue(iv->getPosByChFld(chan, FLD_YD, rec), rec, (int)((p->packet[11+6] << 8) + p->packet[12+6])); // was 11/12, might be wrong!

            //if (INV_TYPE_1CH == iv->type)
                //iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, (int)((p->packet[11+6] << 8) + p->packet[12+6]));

            //iv->setValue(iv->getPosByChFld(chan, FLD_EVT, rec), rec, (int)((p->packet[13] << 8) + p->packet[14]));

            iv->setValue(iv->getPosByChFld(0, FLD_EVT, rec), rec, (int)((p->packet[11] << 8) + p->packet[12]));
            //iv->setValue(iv->getPosByChFld(0, FLD_EVT, rec), rec, (int)((p->packet[14] << 8) + p->packet[16]));
            if (iv->alarmMesIndex < rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]){
                iv->alarmMesIndex = rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]; // seems there's no status per channel in 3rd gen. models?!?

                DPRINTLN(DBG_INFO, "alarm ID incremented to " + String(iv->alarmMesIndex));
                iv->enqueCommand<InfoCommand>(AlarmData);
            }
            /* Unclear how in HM inverters Info and alarm data is handled...
            */


            /*            int8_t offset = -2;

                for decoding see
                void MI600StsMsg (NRF24_packet_t *p){
                  STAT = (int)((p->packet[11] << 8) + p->packet[12]);
                  FCNT = (int)((p->packet[13] << 8) + p->packet[14]);
                  FCODE = (int)((p->packet[15] << 8) + p->packet[16]);
                #ifdef ESP8266
                  VALUES[PV][5]=STAT;
                  VALUES[PV][6]=FCNT;
                  VALUES[PV][7]=FCODE;
                #endif
                }
                */
        }

        void miDataDecode(Inverter<> *iv, packet_t *p) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);  // choose the parser
            rec->ts = mPayload[iv->id].ts;

            uint8_t datachan = ( p->packet[0] == 0x89 || p->packet[0] == (0x36 + ALL_FRAMES) ) ? CH1 :
                           ( p->packet[0] == 0x91 || p->packet[0] == (0x37 + ALL_FRAMES) ) ? CH2 :
                           p->packet[0] == (0x38 + ALL_FRAMES) ? CH3 :
                           CH4;
            int8_t offset = -2;
            // U_DC =  (float) ((p->packet[11] << 8) + p->packet[12])/10;
            iv->setValue(iv->getPosByChFld(datachan, FLD_UDC, rec), rec, (float)((p->packet[11+offset] << 8) + p->packet[12+offset])/10);
            yield();
            // I_DC =  (float) ((p->packet[13] << 8) + p->packet[14])/10;
            iv->setValue(iv->getPosByChFld(datachan, FLD_IDC, rec), rec, (float)((p->packet[13+offset] << 8) + p->packet[14+offset])/10);
            yield();
            //      U_AC =  (float) ((p->packet[15] << 8) + p->packet[16])/10;
            iv->setValue(iv->getPosByChFld(0, FLD_UAC, rec), rec, (float)((p->packet[15+offset] << 8) + p->packet[16+offset])/10);
            yield();
            //      F_AC =  (float) ((p->packet[17] << 8) + p->packet[18])/100;
            //iv->setValue(iv->getPosByChFld(0, FLD_IAC, rec), rec, (float)((p->packet[17+offset] << 8) + p->packet[18+offset])/100);
            //yield();
            //      P_DC =  (float)((p->packet[19] << 8) + p->packet[20])/10;
            iv->setValue(iv->getPosByChFld(datachan, FLD_PDC, rec), rec, (float)((p->packet[19+offset] << 8) + p->packet[20+offset])/10);
            yield();
            //      Q_DC =  (float)((p->packet[21] << 8) + p->packet[22])/1;
            iv->setValue(iv->getPosByChFld(datachan, FLD_YD, rec), rec, (float)((p->packet[21+offset] << 8) + p->packet[22+offset])/1);
            yield();
            iv->setValue(iv->getPosByChFld(0, FLD_T, rec), rec, (float) ((int16_t)(p->packet[23+offset] << 8) + p->packet[24+offset])/10); //23 is freq or IAC?
            iv->setValue(iv->getPosByChFld(0, FLD_F, rec), rec, (float) ((p->packet[17+offset] << 8) + p->packet[18+offset])/100);
            iv->setValue(iv->getPosByChFld(0, FLD_IRR, rec), rec, (float) (calcIrradiation(iv, datachan)));
            yield();
            //FLD_YD

            if (p->packet[0] >= (0x36 + ALL_FRAMES) ) {
                /*status message analysis most liklely needs to be changed, see MiStsMst*/
                /*STAT = (uint8_t)(p->packet[25] );
                FCNT = (uint8_t)(p->packet[26]);
                FCODE = (uint8_t)(p->packet[27]); // MI300/Mi600 stsMsg:: (int)((p->packet[15] << 8) + p->packet[16]); */
                iv->setValue(iv->getPosByChFld(0, FLD_EVT, rec), rec, (uint8_t)(p->packet[25+offset]));
                yield();
                if (iv->alarmMesIndex < rec->record[iv->getPosByChFld(0, FLD_EVT, rec)]){
                    iv->alarmMesIndex = rec->record[iv->getPosByChFld(0, FLD_EVT, rec)];

                    DPRINTLN(DBG_INFO, "alarm ID incremented to " + String(iv->alarmMesIndex));
                    iv->enqueCommand<InfoCommand>(AlarmData);
                }
            }
            //iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, CALC_YD_CH0); // (getValue(iv->getPosByChFld(1, FLD_YD, rec), rec) + getValue(iv->getPosByChFld(2, FLD_YD, rec), rec)));
            iv->setValue(iv->getPosByChFld(0, FLD_YD, rec), rec, calcYieldDayCh0(iv,0)); //datachan));
                            iv->doCalculations();
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
/*decode here or memcopy payload for later decoding?
                void MI600DataMsg(NRF24_packet_t *p){
                  U_DC =  (float) ((p->packet[11] << 8) + p->packet[12])/10;
                  I_DC =  (float) ((p->packet[13] << 8) + p->packet[14])/10;
                  U_AC =  (float) ((p->packet[15] << 8) + p->packet[16])/10;
                  F_AC =  (float) ((p->packet[17] << 8) + p->packet[18])/100;
                  P_DC =  (float)((p->packet[19] << 8) + p->packet[20])/10;
                  Q_DC =  (float)((p->packet[21] << 8) + p->packet[22])/1;
                  TEMP =  (float) ((p->packet[23] << 8) + p->packet[24])/10;  //(int16_t)

                  if ((30<U_DC<50) && (0<I_DC<15) && (200<U_AC<300) && (45<F_AC<55) && (0<P_DC<420) && (0<TEMP<80))
                   DataOK = 1;  //we need to check this, if no crc
                  else { DEBUG_OUT.printf("Data Wrong!!\r\n");DataOK =0; return;}

                  if (p->packet[2] == 0x89)  {PV= 0; TotalP[1]=P_DC; pvCnt[0]=1;}//port 1
                  if (p->packet[2] == 0x91)  {PV= 1; TotalP[2]=P_DC; pvCnt[1]=1;}//port 2

                  TotalP[0]=TotalP[1]+TotalP[2]+TotalP[3]+TotalP[4];//in TotalP[0] is the totalPV power
                  if((P_DC>400) || (P_DC<0) || (TotalP[0]>MAXPOWER)){// cant be!!
                    TotalP[0]=0;
                    return;
                    }
                #ifdef ESP8266
                  VALUES[PV][0]=PV;
                  VALUES[PV][1]=P_DC;
                  VALUES[PV][2]=U_DC;
                  VALUES[PV][3]=I_DC;
                  VALUES[PV][4]=Q_DC;
                #endif
                  PMI=TotalP[0];
                  LIM=(uint16_t)Limit;
                  PrintOutValues();
                }*/

                /*For MI1500:
                if (MI1500) {
  STAT = (uint8_t)(p->packet[25] );
  FCNT = (uint8_t)(p->packet[26]);
  FCODE = (uint8_t)(p->packet[27]);
  }
                */
                DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(": data msg 0x") + String(p->packet[0], HEX) + F(" channel ") + datachan);
        }

        bool build(uint8_t id, bool *complete) {
            /*DPRINTLN(DBG_VERBOSE, F("build"));
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
            mPayload[id].txCmd       = 0;
            mPayload[id].requested   = false;
            mPayload[id].ts          = *mTimestamp;
            mPayload[id].stsa        = false;
            mPayload[id].stsb        = false;
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
        alarmListenerType mCbMiAlarm;
        payloadListenerType mCbMiPayload;
};

#endif /*__MI_PAYLOAD_H__*/
