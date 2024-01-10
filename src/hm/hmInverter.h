//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __HM_INVERTER_H__
#define __HM_INVERTER_H__

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#define MAX_GRID_LENGTH     150

#include "hmDefines.h"
#include "../appInterface.h"
#include "HeuristicInv.h"
#include "../hms/hmsDefines.h"
#include <memory>
#include <queue>
#include <functional>
#include "../config/settings.h"

#include "radio.h"
/**
 * For values which are of interest and not transmitted by the inverter can be
 * calculated automatically.
 * A list of functions can be linked to the assignment and will be executed
 * automatically. Their result does not differ from original read values.
 */


// prototypes
template<class T=float>
static T calcYieldTotalCh0(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcYieldDayCh0(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcUdcCh(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcPowerDcCh0(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcEffiencyCh0(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcIrradiation(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcMaxPowerAcCh0(Inverter<> *iv, uint8_t arg0);

template<class T=float>
static T calcMaxPowerDc(Inverter<> *iv, uint8_t arg0);

template<class T=float>
using func_t = T (Inverter<> *, uint8_t);

template<class T=float>
struct calcFunc_t {
    uint8_t funcId; // unique id
    func_t<T>*  func;   // function pointer
};

enum class MqttSentStatus : uint8_t {
    NEW_DATA,
    LAST_SUCCESS_SENT,
    DATA_SENT
};

enum class InverterStatus : uint8_t {
    OFF,
    STARTING,
    PRODUCING,
    WAS_PRODUCING,
    WAS_ON
};

template<class T=float>
struct record_t {
    byteAssign_t* assign;          // assignment of bytes in payload
    uint8_t length;                // length of the assignment list
    T *record;                     // data pointer
    uint32_t ts;                   // timestamp of last received payload
    uint8_t pyldLen;               // expected payload length for plausibility check
    MqttSentStatus mqttSentStatus; // indicates the current MqTT sent status
};

struct alarm_t {
    uint16_t code;
    uint32_t start;
    uint32_t end;
    alarm_t(uint16_t c, uint32_t s, uint32_t e) : code(c), start(s), end(e) {}
    alarm_t() : code(0), start(0), end(0) {}
};

// list of all available functions, mapped in hmDefines.h
template<class T=float>
const calcFunc_t<T> calcFunctions[] = {
    { CALC_YT_CH0,   &calcYieldTotalCh0 },
    { CALC_YD_CH0,   &calcYieldDayCh0   },
    { CALC_UDC_CH,   &calcUdcCh         },
    { CALC_PDC_CH0,  &calcPowerDcCh0    },
    { CALC_EFF_CH0,  &calcEffiencyCh0   },
    { CALC_IRR_CH,   &calcIrradiation   },
    { CALC_MPAC_CH0, &calcMaxPowerAcCh0 },
    { CALC_MPDC_CH,  &calcMaxPowerDc    }
};

template <class REC_TYP>
class Inverter {
    public:
        uint8_t       ivGen;             // generation of inverter (HM / MI)
        cfgIv_t       *config;           // stored settings
        uint8_t       id;                // unique id
        uint8_t       type;              // integer which refers to inverter type
        uint16_t      alarmMesIndex;     // Last recorded Alarm Message Index
        uint16_t      powerLimit[2];     // limit power output (multiplied by 10)
        float         actPowerLimit;     // actual power limit
        bool          powerLimitAck;     // acknowledged power limit (default: false)
        uint8_t       devControlCmd;     // carries the requested cmd
        serial_u      radioId;           // id converted to modbus
        uint8_t       channels;          // number of PV channels (1-4)
        record_t<REC_TYP> recordMeas;    // structure for measured values
        record_t<REC_TYP> recordInfo;    // structure for info values
        record_t<REC_TYP> recordHwInfo;  // structure for simple (hardware) info values
        record_t<REC_TYP> recordConfig;  // structure for system config values
        record_t<REC_TYP> recordAlarm;   // structure for alarm values
        bool          isConnected;       // shows if inverter was successfully identified (fw version and hardware info)
        InverterStatus status;           // indicates the current inverter status
        std::array<alarm_t, 10> lastAlarm; // holds last 10 alarms
        int8_t        rssi;              // RSSI
        uint16_t      alarmCnt;          // counts the total number of occurred alarms
        uint16_t      alarmLastId;       // lastId which was received
        uint8_t       mCmd;              // holds the command to send
        bool          mGotFragment;      // shows if inverter has sent at least one fragment
        uint8_t       miMultiParts;      // helper info for MI multiframe msgs
        uint8_t       outstandingFrames; // helper info to count difference between expected and received frames
        uint8_t       curFrmCnt;         // count received frames in current loop
        bool          mGotLastMsg;       // shows if inverter has already finished transmission cycle
        bool          mIsSingleframeReq; // indicates this is a missing single frame request
        Radio         *radio;            // pointer to associated radio class
        statistics_t  radioStatistics;   // information about transmitted, failed, ... packets
        HeuristicInv  heuristics;        // heuristic information / logic
        uint8_t       curCmtFreq;        // current used CMT frequency, used to check if freq. was changed during runtime
        bool          commEnabled;       // 'pause night communication' sets this field to false
        uint32_t      tsMaxAcPower;      // holds the timestamp when the MaxAC power was seen

        static uint32_t  *timestamp;     // system timestamp
        static cfgInst_t *generalConfig; // general inverter configuration from setup
        static IApp      *app;           // pointer to app interface

    public:

        Inverter() {
            ivGen              = IV_HM;
            powerLimit[0]      = 0xffff;               // 6553.5 W Limit -> unlimited
            powerLimit[1]      = AbsolutNonPersistent; // default power limit setting
            powerLimitAck      = false;
            actPowerLimit      = 0xffff;               // init feedback from inverter to -1
            mDevControlRequest = false;
            devControlCmd      = InitDataState;
            alarmMesIndex      = 0;
            isConnected        = false;
            status             = InverterStatus::OFF;
            alarmCnt           = 0;
            alarmLastId        = 0;
            rssi               = -127;
            miMultiParts       = 0;
            mGotLastMsg        = false;
            mCmd               = InitDataState;
            mIsSingleframeReq  = false;
            radio              = NULL;
            commEnabled        = true;
            tsMaxAcPower       = 0;

            memset(&radioStatistics, 0, sizeof(statistics_t));
            memset(heuristics.txRfQuality, -6, 5);

            memset(mOffYD, 0, sizeof(float) * 6);
            memset(mLastYD, 0, sizeof(float) * 6);
        }

        void tickSend(std::function<void(uint8_t cmd, bool isDevControl)> cb) {
            if(mDevControlRequest) {
                cb(devControlCmd, true);
                mDevControlRequest = false;
            } else if (IV_MI != ivGen) { // HM / HMS / HMT
                mGetLossInterval++;
                if(mNextLive)
                    cb(RealTimeRunData_Debug, false);    // get live data
                else {
                    if(actPowerLimit == 0xffff)
                        cb(SystemConfigPara, false);         // power limit info
                    else if(InitDataState != devControlCmd) {
                        cb(devControlCmd, false);            // custom command which was received by API
                        devControlCmd = InitDataState;
                        mGetLossInterval = 1;
                    } else if(0 == getFwVersion())
                        cb(InverterDevInform_All, false);    // get firmware version
                    else if(0 == getHwVersion())
                        cb(InverterDevInform_Simple, false); // get hardware version
                    else if((alarmLastId != alarmMesIndex) && (alarmMesIndex != 0))
                        cb(AlarmData, false);                // get last alarms
                    else if((0 == mGridLen) && generalConfig->readGrid) { // read grid profile
                        cb(GridOnProFilePara, false);
                    } else if (mGetLossInterval > AHOY_GET_LOSS_INTERVAL) { // get loss rate
                        mGetLossInterval = 1;
                        cb(GetLossRate, false);
                    } else
                        cb(RealTimeRunData_Debug, false);    // get live data
                }
            } else { // MI
                if(0 == getFwVersion()) {
                    mIvRxCnt +=2;
                    cb(0x0f, false);    // get firmware version; for MI, this makes part of polling the device software and hardware version number
                } else {
                    record_t<> *rec = getRecordStruct(InverterDevInform_Simple);
                    if (getChannelFieldValue(CH0, FLD_PART_NUM, rec) == 0) {
                        cb(0x0f, false); // hard- and firmware version for missing HW part nr, delivered by frame 1
                        mIvRxCnt +=2;
                    } else if((getChannelFieldValue(CH0, FLD_GRID_PROFILE_CODE, rec) == 0) && generalConfig->readGrid) // read grid profile
                        cb(0x10, false); // legacy GPF command
                    else {
                        cb(((type == INV_TYPE_4CH) ? MI_REQ_4CH : MI_REQ_CH1), false);
                        mGetLossInterval++;
                        if (type != INV_TYPE_4CH)
                            mIvRxCnt++;  // statistics workaround...
                    }
                }
            }
        }

        void init(void) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:init"));
            initAssignment(&recordMeas, RealTimeRunData_Debug);
            initAssignment(&recordInfo, InverterDevInform_All);
            initAssignment(&recordHwInfo, InverterDevInform_Simple);
            initAssignment(&recordConfig, SystemConfigPara);
            initAssignment(&recordAlarm, AlarmData);
            toRadioId();
            curCmtFreq = this->config->frequency; // update to frequency read from settings
        }

        uint8_t getPosByChFld(uint8_t channel, uint8_t fieldId, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getPosByChFld"));
            uint8_t pos = 0;
            if(NULL != rec) {
                for(; pos < rec->length; pos++) {
                    if((rec->assign[pos].ch == channel) && (rec->assign[pos].fieldId == fieldId))
                        break;
                }
                return (pos >= rec->length) ? 0xff : pos;
            }
            else
                return 0xff;
        }

        byteAssign_t *getByteAssign(uint8_t pos, record_t<> *rec) {
            return &rec->assign[pos];
        }

        const char *getFieldName(uint8_t pos, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getFieldName"));
            if(NULL != rec)
                return fields[rec->assign[pos].fieldId];
            return notAvail;
        }

        const char *getUnit(uint8_t pos, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getUnit"));
            if(NULL != rec)
                return units[rec->assign[pos].unitId];
            return notAvail;
        }

        uint8_t getChannel(uint8_t pos, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getChannel"));
            if(NULL != rec)
                return rec->assign[pos].ch;
            return 0;
        }

        bool setDevControlRequest(uint8_t cmd) {
            if(isConnected) {
                mDevControlRequest = true;
                devControlCmd = cmd;
                app->triggerTickSend();
            }
            return isConnected;
        }

        bool setDevCommand(uint8_t cmd) {
            if(isConnected)
                devControlCmd = cmd;
            return isConnected;
        }

        void addValue(uint8_t pos, uint8_t buf[], record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:addValue"));
            if(NULL != rec) {
                uint8_t  ptr = rec->assign[pos].start;
                uint8_t  end = ptr + rec->assign[pos].num;
                uint16_t div = rec->assign[pos].div;

                if(NULL != rec) {
                    if(CMD_CALC != div) {
                        uint32_t val = 0;
                        do {
                            val <<= 8;
                            val |= buf[ptr];
                        } while(++ptr != end);
                        if ((FLD_T == rec->assign[pos].fieldId) || (FLD_Q == rec->assign[pos].fieldId) || (FLD_PF == rec->assign[pos].fieldId)) {
                            // temperature, Qvar, and power factor are a signed values
                            rec->record[pos] = ((REC_TYP)((int16_t)val)) / (REC_TYP)(div);
                        } else if (FLD_YT == rec->assign[pos].fieldId) {
                            rec->record[pos] = ((REC_TYP)(val) / (REC_TYP)(div) * generalConfig->yieldEffiency) + ((REC_TYP)config->yieldCor[rec->assign[pos].ch-1]);
                        } else if (FLD_YD == rec->assign[pos].fieldId) {
                            float actYD = (REC_TYP)(val) / (REC_TYP)(div) * generalConfig->yieldEffiency;
                            uint8_t idx = rec->assign[pos].ch - 1;
                            if (mLastYD[idx] > actYD)
                                mOffYD[idx] += mLastYD[idx];
                            mLastYD[idx] = actYD;
                            rec->record[pos] = mOffYD[idx] + actYD;
                        } else {
                            if ((REC_TYP)(div) > 1)
                                rec->record[pos] = (REC_TYP)(val) / (REC_TYP)(div);
                            else
                                rec->record[pos] = (REC_TYP)(val);
                        }
                    }
                }

                if(rec == &recordMeas) {
                    mNextLive = false; // live data received
                    DPRINTLN(DBG_VERBOSE, "add real time");
                    // get last alarm message index and save it in the inverter object
                    if (getPosByChFld(0, FLD_EVT, rec) == pos) {
                        if (alarmMesIndex < rec->record[pos]) {
                            alarmMesIndex = rec->record[pos];

                            DPRINT(DBG_INFO, "alarm ID incremented to ");
                            DBGPRINTLN(String(alarmMesIndex));
                        }
                    }
                }
                else {
                    mNextLive = true;
                    if (rec->assign == InfoAssignment) {
                        DPRINTLN(DBG_DEBUG, "add info");
                        // eg. fw version ...
                        isConnected = true;
                    } else if (rec->assign == SimpleInfoAssignment) {
                        DPRINTLN(DBG_DEBUG, "add simple info");
                        // eg. hw version ...
                    } else if (rec->assign == SystemConfigParaAssignment) {
                        DPRINTLN(DBG_DEBUG, "add config");
                        if (getPosByChFld(0, FLD_ACT_ACTIVE_PWR_LIMIT, rec) == pos){
                            actPowerLimit = rec->record[pos];
                            DPRINT(DBG_DEBUG, F("Inverter actual power limit: "));
                            DPRINTLN(DBG_DEBUG, String(actPowerLimit, 1));
                        }
                    } else if (rec->assign == AlarmDataAssignment) {
                        DPRINTLN(DBG_DEBUG, "add alarm");
                    } else
                        DPRINTLN(DBG_WARN, F("add with unknown assignment"));
                }
            }
            else
                DPRINTLN(DBG_ERROR, F("addValue: assignment not found with cmd 0x"));

            // update status state-machine
            isProducing();
        }

        bool setValue(uint8_t pos, record_t<> *rec, REC_TYP val) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:setValue"));
            if(NULL == rec)
                return false;
            if(pos > rec->length)
                return false;
            rec->record[pos] = val;
            return true;
        }

        REC_TYP getChannelFieldValue(uint8_t channel, uint8_t fieldId, record_t<> *rec) {
            uint8_t pos = 0;
            if(NULL != rec) {
                for(; pos < rec->length; pos++) {
                    if((rec->assign[pos].ch == channel) && (rec->assign[pos].fieldId == fieldId))
                        break;
                }
                if(pos >= rec->length)
                    return 0;

                return rec->record[pos];
            }
            else
                return 0;
        }

        uint32_t getChannelFieldValueInt(uint8_t channel, uint8_t fieldId, record_t<> *rec) {
            return (uint32_t) getChannelFieldValue(channel, fieldId, rec);
        }

        REC_TYP getValue(uint8_t pos, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getValue"));
            if(NULL == rec)
                return 0;
            if(pos > rec->length)
                return 0;
            return rec->record[pos];
        }

        void doCalculations() {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:doCalculations"));
            record_t<> *rec = getRecordStruct(RealTimeRunData_Debug);
            for(uint8_t i = 0; i < rec->length; i++) {
                if(CMD_CALC == rec->assign[i].div) {
                    rec->record[i] = calcFunctions<REC_TYP>[rec->assign[i].start].func(this, rec->assign[i].num);
                }
                yield();
            }
        }

        bool isAvailable() {
            bool avail = false;

            if((recordMeas.ts == 0) && (recordInfo.ts == 0) && (recordConfig.ts == 0) && (recordAlarm.ts == 0))
                return false;

            if((*timestamp - recordMeas.ts) < INVERTER_INACT_THRES_SEC)
                avail = true;
            if((*timestamp - recordInfo.ts) < INVERTER_INACT_THRES_SEC)
                avail = true;
            if((*timestamp - recordConfig.ts) < INVERTER_INACT_THRES_SEC)
                avail = true;
            if((*timestamp - recordAlarm.ts) < INVERTER_INACT_THRES_SEC)
                avail = true;

            if(avail) {
                if(status < InverterStatus::PRODUCING)
                    status = InverterStatus::STARTING;
            } else {
                if((*timestamp - recordMeas.ts) > INVERTER_OFF_THRES_SEC) {
                    status = InverterStatus::OFF;
                    actPowerLimit = 0xffff; // power limit will be read once inverter becomes available
                    alarmMesIndex = 0;
                }
                else
                    status = InverterStatus::WAS_ON;
            }

            return avail;
        }

        bool isProducing() {
            bool producing = false;
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:isProducing"));
            if(isAvailable()) {
                producing = (getChannelFieldValue(CH0, FLD_PAC, &recordMeas) > INACT_PWR_THRESH);

                if(producing)
                    status = InverterStatus::PRODUCING;
                else if(InverterStatus::PRODUCING == status)
                    status = InverterStatus::WAS_PRODUCING;
            }
            return producing;
        }

        InverterStatus getStatus(){
            isProducing(); // recalculate status
            return status;
        }

        uint16_t getFwVersion() {
            record_t<> *rec = getRecordStruct(InverterDevInform_All);
            return getChannelFieldValue(CH0, FLD_FW_VERSION, rec);
        }

        uint16_t getHwVersion() {
            record_t<> *rec = getRecordStruct(InverterDevInform_Simple);
            return getChannelFieldValue(CH0, FLD_HW_VERSION, rec);
        }

        uint16_t getMaxPower() {
            record_t<> *rec = getRecordStruct(InverterDevInform_Simple);
            if(0 == getChannelFieldValue(CH0, FLD_HW_VERSION, rec))
                return 0;

            for(uint8_t i = 0; i < sizeof(devInfo) / sizeof(devInfo_t); i++) {
                if(devInfo[i].hwPart == (getChannelFieldValueInt(CH0, FLD_PART_NUM, rec) >> 8))
                    return devInfo[i].maxPower;
            }
            return 0;
        }

        uint32_t getLastTs(record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getLastTs"));
            return rec->ts;
        }

        record_t<> *getRecordStruct(uint8_t cmd) {
            switch (cmd) {
                case RealTimeRunData_Debug:    return &recordMeas;   // 11 = 0x0b
                case InverterDevInform_Simple: return &recordHwInfo; //  0 = 0x00
                case InverterDevInform_All:    return &recordInfo;   //  1 = 0x01
                case SystemConfigPara:         return &recordConfig; //  5 = 0x05
                case AlarmData:                return &recordAlarm;  // 17 = 0x11
                default:                       break;
            }
            return NULL;
        }

        void initAssignment(record_t<> *rec, uint8_t cmd) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:initAssignment"));
            rec->ts     = 0;
            rec->length = 0;
            rec->mqttSentStatus = MqttSentStatus::DATA_SENT; // nothing new to transmit
            switch (cmd) {
                case RealTimeRunData_Debug:
                    if (INV_TYPE_1CH == type) {
                        if((IV_HM == ivGen) || (IV_MI == ivGen)) {
                            rec->length  = (uint8_t)(HM1CH_LIST_LEN);
                            rec->assign  = (byteAssign_t *)hm1chAssignment;
                            rec->pyldLen = HM1CH_PAYLOAD_LEN;
                        } else if(IV_HMS == ivGen) {
                            rec->length  = (uint8_t)(HMS1CH_LIST_LEN);
                            rec->assign  = (byteAssign_t *)hms1chAssignment;
                            rec->pyldLen = HMS1CH_PAYLOAD_LEN;
                        }
                        channels = 1;
                    }
                    else if (INV_TYPE_2CH == type) {
                        if((IV_HM == ivGen) || (IV_MI == ivGen)) {
                            rec->length  = (uint8_t)(HM2CH_LIST_LEN);
                            rec->assign  = (byteAssign_t *)hm2chAssignment;
                            rec->pyldLen = HM2CH_PAYLOAD_LEN;
                        } else if(IV_HMS == ivGen) {
                            rec->length  = (uint8_t)(HMS2CH_LIST_LEN);
                            rec->assign  = (byteAssign_t *)hms2chAssignment;
                            rec->pyldLen = HMS2CH_PAYLOAD_LEN;
                        }
                        channels = 2;
                    }
                    else if (INV_TYPE_4CH == type) {
                        if((IV_HM == ivGen) || (IV_MI == ivGen)) {
                            rec->length  = (uint8_t)(HM4CH_LIST_LEN);
                            rec->assign  = (byteAssign_t *)hm4chAssignment;
                            rec->pyldLen = HM4CH_PAYLOAD_LEN;
                        } else if(IV_HMS == ivGen) {
                            rec->length  = (uint8_t)(HMS4CH_LIST_LEN);
                            rec->assign  = (byteAssign_t *)hms4chAssignment;
                            rec->pyldLen = HMS4CH_PAYLOAD_LEN;
                        }
                        channels = 4;
                    }
                    else if (INV_TYPE_6CH == type) {
                        rec->length  = (uint8_t)(HMT6CH_LIST_LEN);
                        rec->assign  = (byteAssign_t *)hmt6chAssignment;
                        rec->pyldLen = HMT6CH_PAYLOAD_LEN;
                        channels = 6;
                    }
                    else {
                        rec->length  = 0;
                        rec->assign  = NULL;
                        rec->pyldLen = 0;
                        channels     = 0;
                    }
                    break;
                case InverterDevInform_All:
                    rec->length  = (uint8_t)(HMINFO_LIST_LEN);
                    rec->assign  = (byteAssign_t *)InfoAssignment;
                    rec->pyldLen = HMINFO_PAYLOAD_LEN;
                    break;
                case InverterDevInform_Simple:
                    rec->length  = (uint8_t)(HMSIMPLE_INFO_LIST_LEN);
                    rec->assign  = (byteAssign_t *)SimpleInfoAssignment;
                    rec->pyldLen = HMSIMPLE_INFO_PAYLOAD_LEN;
                    break;
                case SystemConfigPara:
                    rec->length  = (uint8_t)(HMSYSTEM_LIST_LEN);
                    rec->assign  = (byteAssign_t *)SystemConfigParaAssignment;
                    rec->pyldLen = HMSYSTEM_PAYLOAD_LEN;
                    break;
                case AlarmData:
                    rec->length  = (uint8_t)(HMALARMDATA_LIST_LEN);
                    rec->assign  = (byteAssign_t *)AlarmDataAssignment;
                    rec->pyldLen = HMALARMDATA_PAYLOAD_LEN;
                    break;
                default:
                    DPRINTLN(DBG_INFO, F("initAssignment: Parser not implemented"));
                    break;
            }

            if(0 != rec->length) {
                rec->record = new REC_TYP[rec->length];
                memset(rec->record, 0, sizeof(REC_TYP) * rec->length);
            }
        }

        void resetAlarms() {
            lastAlarm.fill({0, 0, 0});
            mAlarmNxtWrPos = 0;
            alarmCnt = 0;
            alarmLastId = 0;

            memset(mOffYD, 0, sizeof(float) * 6);
            memset(mLastYD, 0, sizeof(float) * 6);
        }

        bool parseGetLossRate(uint8_t pyld[], uint8_t len) {
            if (len == HMGETLOSSRATE_PAYLOAD_LEN) {
                uint16_t rxCnt = (pyld[0] << 8) + pyld[1];
                uint16_t txCnt = (pyld[2] << 8) + pyld[3];

                if (mIvRxCnt || mIvTxCnt) { // there was successful GetLossRate in the past
                    radioStatistics.ivSent = mDtuTxCnt;
                    if (rxCnt < mIvRxCnt)   // overflow
                        radioStatistics.ivLoss = radioStatistics.ivSent - (rxCnt + ((uint16_t)65535 - mIvRxCnt) + 1);
                    else
                        radioStatistics.ivLoss = radioStatistics.ivSent - (rxCnt - mIvRxCnt);

                    if (txCnt < mIvTxCnt)   // overflow
                        radioStatistics.dtuSent = txCnt + ((uint16_t)65535 - mIvTxCnt) + 1;
                    else
                        radioStatistics.dtuSent = txCnt - mIvTxCnt;

                    radioStatistics.dtuLoss = radioStatistics.dtuSent - mDtuRxCnt;

                    DPRINT_IVID(DBG_INFO, id);
                    DBGPRINT(F("Inv loss: "));
                    DBGPRINT(String(radioStatistics.ivLoss));
                    DBGPRINT(F(" of "));
                    DBGPRINT(String(radioStatistics.ivSent));
                    DBGPRINT(F(", DTU loss: "));
                    DBGPRINT(String(radioStatistics.dtuLoss));
                    DBGPRINT(F(" of "));
                    DBGPRINTLN(String(radioStatistics.dtuSent));
                }

                mIvRxCnt  = rxCnt;
                mIvTxCnt  = txCnt;
                mDtuRxCnt = 0;  // start new interval
                mDtuTxCnt = 0;  // start new interval
                return true;
            }

            return false;
        }

        uint16_t parseAlarmLog(uint8_t id, uint8_t pyld[], uint8_t len) {
            uint8_t startOff = 2 + id * ALARM_LOG_ENTRY_SIZE;
            if((startOff + ALARM_LOG_ENTRY_SIZE) > len)
                return 0;

            uint16_t wCode = ((uint16_t)pyld[startOff]) << 8 | pyld[startOff+1];
            uint32_t startTimeOffset = 0, endTimeOffset = 0;
            uint32_t start, endTime;

            // check if is AM or PM
            startTimeOffset = ((wCode >> 13) & 0x01) * 12 * 60 * 60;
            if (((wCode >> 12) & 0x03) != 0)
                endTimeOffset = 12 * 60 * 60;

            start     = (((uint16_t)pyld[startOff + 4] << 8) | ((uint16_t)pyld[startOff + 5])) + startTimeOffset;
            endTime   = (((uint16_t)pyld[startOff + 6] << 8) | ((uint16_t)pyld[startOff + 7])) + endTimeOffset;

            DPRINTLN(DBG_DEBUG, "Alarm #" + String(pyld[startOff+1]) + " '" + String(getAlarmStr(pyld[startOff+1])) + "' start: " + ah::getTimeStr(start) + ", end: " + ah::getTimeStr(endTime));
            addAlarm(pyld[startOff+1], start, endTime);

            alarmCnt++;
            alarmLastId = alarmMesIndex;

            return pyld[startOff+1];
        }

        static String getAlarmStr(uint16_t alarmCode) {
            switch (alarmCode) { // breaks are intentionally missing!
                case 1:    return String(F("Inverter start"));
                case 2:    return String(F("Time calibration"));
                case 3:    return String(F("EEPROM reading and writing error during operation"));
                case 4:    return String(F("Offline"));
                case 11:   return String(F("Grid voltage surge"));
                case 12:   return String(F("Grid voltage sharp drop"));
                case 13:   return String(F("Grid frequency mutation"));
                case 14:   return String(F("Grid phase mutation"));
                case 15:   return String(F("Grid transient fluctuation"));
                case 36:   return String(F("INV overvoltage or overcurrent"));
                case 46:   return String(F("FB overvoltage"));
                case 47:   return String(F("FB overcurrent"));
                case 48:   return String(F("FB clamp overvoltage"));
                case 49:   return String(F("FB clamp overvoltage"));
                case 61:   return String(F("Calibration parameter error"));
                case 62:   return String(F("System configuration parameter error"));
                case 63:   return String(F("Abnormal power generation data"));

                case 71:   return String(F("Grid overvoltage load reduction (VW) function enable"));
                case 72:   return String(F("Power grid over-frequency load reduction (FW) function enable"));
                case 73:   return String(F("Over-temperature load reduction (TW) function enable"));

                case 95:   return String(F("PV-1: Module in suspected shadow"));
                case 96:   return String(F("PV-2: Module in suspected shadow"));
                case 97:   return String(F("PV-3: Module in suspected shadow"));
                case 98:   return String(F("PV-4: Module in suspected shadow"));

                case 121:  return String(F("Over temperature protection"));
                case 122:  return String(F("Microinverter is suspected of being stolen"));
                case 123:  return String(F("Locked by remote control"));
                case 124:  return String(F("Shut down by remote control"));
                case 125:  return String(F("Grid configuration parameter error"));
                case 126:  return String(F("EEPROM reading and writing error"));
                case 127:  return String(F("Firmware error"));
                case 128:  return String(F("Hardware configuration error"));
                case 129:  return String(F("Abnormal bias"));
                case 130:  return String(F("Offline"));
                case 141:  return String(F("Grid: Grid overvoltage"));
                case 142:  return String(F("Grid: 10 min value grid overvoltage"));
                case 143:  return String(F("Grid: Grid undervoltage"));
                case 144:  return String(F("Grid: Grid overfrequency"));
                case 145:  return String(F("Grid: Grid underfrequency"));
                case 146:  return String(F("Grid: Rapid grid frequency change rate"));
                case 147:  return String(F("Grid: Power grid outage"));
                case 148:  return String(F("Grid: Grid disconnection"));
                case 149:  return String(F("Grid: Island detected"));

                case 150:  return String(F("DCI exceeded"));

                case 171:  return String(F("Grid: Abnormal phase difference between phase to phase"));
                case 181:  return String(F("Abnormal insulation impedance"));
                case 182:  return String(F("Abnormal grounding"));
                case 205:  return String(F("MPPT-A: Input overvoltage"));
                case 206:  return String(F("MPPT-B: Input overvoltage"));
                case 207:  return String(F("MPPT-A: Input undervoltage"));
                case 208:  return String(F("MPPT-B: Input undervoltage"));
                case 209:  return String(F("PV-1: No input"));
                case 210:  return String(F("PV-2: No input"));
                case 211:  return String(F("PV-3: No input"));
                case 212:  return String(F("PV-4: No input"));
                case 213:  return String(F("MPPT-A: PV-1 & PV-2 abnormal wiring"));
                case 214:  return String(F("MPPT-B: PV-3 & PV-4 abnormal wiring"));
                case 215:  return String(F("MPPT-C: Input overvoltage"));
                case 216:  return String(F("PV-1: Input undervoltage"));
                case 217:  return String(F("PV-2: Input overvoltage"));
                case 218:  return String(F("PV-2: Input undervoltage"));
                case 219:  return String(F("PV-3: Input overvoltage"));
                case 220:  return String(F("PV-3: Input undervoltage"));
                case 221:  return String(F("PV-4: Input overvoltage"));
                case 222:  return String(F("PV-4: Input undervoltage"));

                case 301:  return String(F("FB short circuit failure"));
                case 302:  return String(F("FB short circuit failure"));
                case 303:  return String(F("FB overcurrent protection failure"));
                case 304:  return String(F("FB overcurrent protection failure"));
                case 305:  return String(F("FB clamp circuit failure"));
                case 306:  return String(F("FB clamp circuit failure"));
                case 307:  return String(F("INV power device failure"));
                case 308:  return String(F("INV overcurrent or overvoltage protection failure"));
                case 309:  return String(F("Hardware error code 309"));
                case 310:  return String(F("Hardware error code 310"));
                case 311:  return String(F("Hardware error code 311"));
                case 312:  return String(F("Hardware error code 312"));
                case 313:  return String(F("Hardware error code 313"));
                case 314:  return String(F("Hardware error code 314"));

                case 5011: return String(F("PV-1: MOSFET overcurrent (II)"));
                case 5012: return String(F("PV-2: MOSFET overcurrent (II)"));
                case 5013: return String(F("PV-3: MOSFET overcurrent (II)"));
                case 5014: return String(F("PV-4: MOSFET overcurrent (II)"));
                case 5020: return String(F("H-bridge MOSFET overcurrent or H-bridge overvoltage"));

                case 5041: return String(F("PV-1: current overcurrent (II)"));
                case 5042: return String(F("PV-2: current overcurrent (II)"));
                case 5043: return String(F("PV-3: current overcurrent (II)"));
                case 5044: return String(F("PV-4: current overcurrent (II)"));

                case 5051: return String(F("PV Input 1 Overvoltage/Undervoltage"));
                case 5052: return String(F("PV Input 2 Overvoltage/Undervoltage"));
                case 5053: return String(F("PV Input 3 Overvoltage/Undervoltage"));
                case 5054: return String(F("PV Input 4 Overvoltage/Undervoltage"));
                case 5060: return String(F("Abnormal bias"));
                case 5070: return String(F("Over temperature protection"));
                case 5080: return String(F("Grid Overvoltage/Undervoltage"));
                case 5090: return String(F("Grid Overfrequency/Underfrequency"));
                case 5100: return String(F("Island detected"));
                case 5110: return String(F("GFDI"));
                case 5120: return String(F("EEPROM reading and writing error"));
                case 5141:
                case 5142:
                case 5143:
                case 5144:
                           return String(F("FB clamp overvoltage"));
                case 5150: return String(F("10 min value grid overvoltage"));
                case 5160: return String(F("Grid transient fluctuation"));
                case 5200: return String(F("Firmware error"));
                case 8310: return String(F("Shut down by remote control"));
                case 8320: return String(F("Locked by remote control"));
                case 9000: return String(F("Microinverter is suspected of being stolen"));
                default:   return String(F("Unknown"));
            }
        }

        void addGridProfile(uint8_t buf[], uint8_t length) {
            mGridLen = (length > MAX_GRID_LENGTH) ? MAX_GRID_LENGTH : length;
            std::copy(buf, &buf[mGridLen], mGridProfile);
        }

        String getGridProfile(void) {
            char buf[MAX_GRID_LENGTH * 3];
            memset(buf, 0, MAX_GRID_LENGTH);
            for(uint8_t i = 0; i < mGridLen; i++) {
                snprintf(&buf[i*3], 4, "%02X ", mGridProfile[i]);
            }
            buf[mGridLen*3] = 0;
            return String(buf);
        }

    private:
        inline void addAlarm(uint16_t code, uint32_t start, uint32_t end) {
            lastAlarm[mAlarmNxtWrPos] = alarm_t(code, start, end);
            if(++mAlarmNxtWrPos >= 10) // rolling buffer
                mAlarmNxtWrPos = 0;
        }

        void toRadioId(void) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:toRadioId"));
            radioId.u64  = 0ULL;
            radioId.b[4] = config->serial.b[0];
            radioId.b[3] = config->serial.b[1];
            radioId.b[2] = config->serial.b[2];
            radioId.b[1] = config->serial.b[3];
            radioId.b[0] = 0x01;
        }

    private:
        float mOffYD[6], mLastYD[6];
        bool mDevControlRequest; // true if change needed
        uint8_t mGridLen = 0;
        uint8_t mGridProfile[MAX_GRID_LENGTH];
        uint8_t mAlarmNxtWrPos = 0; // indicates the position in array (rolling buffer)
        bool mNextLive = true; // first read live data after booting up then version etc.

    public:
        uint16_t mDtuRxCnt = 0;
        uint16_t mDtuTxCnt = 0;
        uint8_t  mGetLossInterval = 0;  // request iv every AHOY_GET_LOSS_INTERVAL RealTimeRunData_Debug
        uint16_t mIvRxCnt  = 0;
        uint16_t mIvTxCnt  = 0;

};

template <class REC_TYP>
uint32_t *Inverter<REC_TYP>::timestamp {0};
template <class REC_TYP>
cfgInst_t *Inverter<REC_TYP>::generalConfig {0};


/**
 * To calculate values which are not transmitted by the unit there is a generic
 * list of functions which can be linked to the assignment.
 * The special command 0xff (CMDFF) must be used.
 */

template<class T=float>
static T calcYieldTotalCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcYieldTotalCh0"));
    if(NULL != iv) {
        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        T yield = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            yield += iv->getChannelFieldValue(i, FLD_YT, rec);
        }
        return yield;
    }
    return 0.0;
}

template<class T=float>
static T calcYieldDayCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcYieldDayCh0"));
    if(NULL != iv) {
        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        T yield = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            yield += iv->getChannelFieldValue(i, FLD_YD, rec);
        }
        return yield;
    }
    return 0.0;
}

template<class T=float>
static T calcUdcCh(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcUdcCh"));
    // arg0 = channel of source
    record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
    for(uint8_t i = 0; i < rec->length; i++) {
        if((FLD_UDC == rec->assign[i].fieldId) && (arg0 == rec->assign[i].ch)) {
            return iv->getValue(i, rec);
        }
    }

    return 0.0;
}

template<class T=float>
static T calcPowerDcCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcPowerDcCh0"));
    if(NULL != iv) {
        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        T dcPower = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            dcPower += iv->getChannelFieldValue(i, FLD_PDC, rec);
        }
        return dcPower;
    }
    return 0.0;
}

template<class T=float>
static T calcEffiencyCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcEfficiencyCh0"));
    if(NULL != iv) {
        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        T acPower = iv->getChannelFieldValue(CH0, FLD_PAC, rec);
        T dcPower = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            dcPower += iv->getChannelFieldValue(i, FLD_PDC, rec);
        }
        if(dcPower > 0)
            return acPower / dcPower * 100.0f;
    }
    return 0.0;
}

template<class T=float>
static T calcIrradiation(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcIrradiation"));
    // arg0 = channel
    if(NULL != iv) {
        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        if(iv->config->chMaxPwr[arg0-1] > 0)
            return iv->getChannelFieldValue(arg0, FLD_PDC, rec) / iv->config->chMaxPwr[arg0-1] * 100.0f;
    }
    return 0.0;
}

template<class T=float>
static T calcMaxPowerAcCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcMaxPowerAcCh0"));
    T acMaxPower = 0.0;
    if(NULL != iv) {
        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        T acPower = iv->getChannelFieldValue(arg0, FLD_PAC, rec);

        for(uint8_t i = 0; i < rec->length; i++) {
            if((FLD_MP == rec->assign[i].fieldId) && (0 == rec->assign[i].ch)) {
                acMaxPower = iv->getValue(i, rec);
            }
        }
        if(acPower > acMaxPower)
            return acPower;
    }
    return acMaxPower;
}

template<class T=float>
static T calcMaxPowerDc(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcMaxPowerDc"));
    // arg0 = channel
    T dcMaxPower = 0.0;
    if(NULL != iv) {
        record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
        T dcPower = iv->getChannelFieldValue(arg0, FLD_PDC, rec);

        for(uint8_t i = 0; i < rec->length; i++) {
            if((FLD_MP == rec->assign[i].fieldId) && (arg0 == rec->assign[i].ch)) {
                dcMaxPower = iv->getValue(i, rec);
            }
        }
        if(dcPower > dcMaxPower) {
            iv->tsMaxAcPower = *iv->timestamp;
            return dcPower;
        }
    }
    return dcMaxPower;
}

#endif /*__HM_INVERTER_H__*/
