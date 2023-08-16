//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HM_INVERTER_H__
#define __HM_INVERTER_H__

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "hmDefines.h"
#include "../hms/hmsDefines.h"
#include <memory>
#include <queue>
#include "../config/settings.h"

/**
 * For values which are of interest and not transmitted by the inverter can be
 * calculated automatically.
 * A list of functions can be linked to the assignment and will be executed
 * automatically. Their result does not differ from original read values.
 */

// forward declaration of class
template <class REC_TYP=float>
class Inverter;


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

template<class T=float>
struct record_t {
    byteAssign_t* assign; // assigment of bytes in payload
    uint8_t length;       // length of the assignment list
    T *record;            // data pointer
    uint32_t ts;          // timestamp of last received payload
    uint8_t pyldLen;      // expected payload length for plausibility check
};

struct alarm_t {
    uint16_t code;
    uint32_t start;
    uint32_t end;
    alarm_t(uint16_t c, uint32_t s, uint32_t e) : code(c), start(s), end(e) {}
    alarm_t() : code(0), start(0), end(0) {}
};

class CommandAbstract {
    public:
        CommandAbstract(uint8_t txType = 0, uint8_t cmd = 0) {
            _TxType = txType;
            _Cmd = cmd;
        };
        virtual ~CommandAbstract() {};

        const uint8_t getCmd() {
            return _Cmd;
        }

    protected:
        uint8_t _TxType;
        uint8_t _Cmd;
};

class InfoCommand : public CommandAbstract {
    public:
        InfoCommand(uint8_t cmd){
            _TxType = 0x15;
            _Cmd = cmd;
        }
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

enum class InverterStatus : uint8_t {
    OFF,
    STARTING,
    PRODUCING,
    WAS_PRODUCING,
    WAS_ON
};

template <class REC_TYP>
class Inverter {
    public:
        uint8_t       ivGen;             // generation of inverter (HM / MI)
        cfgIv_t       *config;           // stored settings
        uint8_t       id;                // unique id
        uint8_t       type;              // integer which refers to inverter type
        uint16_t      alarmMesIndex;     // Last recorded Alarm Message Index
        uint16_t      powerLimit[2];     // limit power output
        float         actPowerLimit;     // actual power limit
        bool          powerLimitAck;     // acknowledged power limit (default: false)
        uint8_t       devControlCmd;     // carries the requested cmd
        serial_u      radioId;           // id converted to modbus
        uint8_t       channels;          // number of PV channels (1-4)
        record_t<REC_TYP> recordMeas;    // structure for measured values
        record_t<REC_TYP> recordInfo;    // structure for info values
        record_t<REC_TYP> recordConfig;  // structure for system config values
        record_t<REC_TYP> recordAlarm;   // structure for alarm values
        //String        lastAlarmMsg;
        bool          initialized;       // needed to check if the inverter was correctly added (ESP32 specific - union types are never null)
        bool          isConnected;       // shows if inverter was successfully identified (fw version and hardware info)
        InverterStatus status;           // indicates the current inverter status
        std::array<alarm_t, 10> lastAlarm; // holds last 10 alarms
        uint8_t       alarmNxtWrPos;     // indicates the position in array (rolling buffer)
        uint16_t      alarmCnt;          // counts the total number of occured alarms


        static uint32_t *timestamp;      // system timestamp
        static cfgInst_t *generalConfig; // general inverter configuration from setup

        Inverter() {
            ivGen              = IV_HM;
            powerLimit[0]      = 0xffff;               // 65535 W Limit -> unlimited
            powerLimit[1]      = AbsolutNonPersistent; // default power limit setting
            powerLimitAck      = false;
            actPowerLimit      = 0xffff;               // init feedback from inverter to -1
            mDevControlRequest = false;
            devControlCmd      = InitDataState;
            initialized        = false;
            //lastAlarmMsg       = "nothing";
            alarmMesIndex      = 0;
            isConnected        = false;
            status             = InverterStatus::OFF;
            alarmNxtWrPos      = 0;
            alarmCnt           = 0;
        }

        ~Inverter() {
            // TODO: cleanup
        }

        template <typename T>
        void enqueCommand(uint8_t cmd) {
           _commandQueue.push(std::make_shared<T>(cmd));
           DPRINT_IVID(DBG_INFO, id);
           DBGPRINT(F("enqueCommand: 0x"));
           DBGHEXLN(cmd);
        }

        void setQueuedCmdFinished() {
            if (!_commandQueue.empty()) {
                // Will destroy CommandAbstract Class Object (?)
                _commandQueue.pop();
            }
        }

        void clearCmdQueue() {
            DPRINTLN(DBG_INFO, F("clearCmdQueue"));
            while (!_commandQueue.empty()) {
                // Will destroy CommandAbstract Class Object (?)
                _commandQueue.pop();
            }
        }

        uint8_t getQueuedCmd() {
            if (_commandQueue.empty()) {
                if (ivGen != IV_MI) {
                    if (getFwVersion() == 0)
                        enqueCommand<InfoCommand>(InverterDevInform_All); // firmware version
                    enqueCommand<InfoCommand>(RealTimeRunData_Debug);  // live data
                } else if (ivGen == IV_MI){
                    if (getFwVersion() == 0)
                        enqueCommand<InfoCommand>(InverterDevInform_All); // firmware version; might not work, esp. for 1/2 ch hardware
                    if (type == INV_TYPE_4CH) {
                        enqueCommand<InfoCommand>(0x36);
                    } else {
                        enqueCommand<InfoCommand>(0x09);
                    }
                }

                if ((actPowerLimit == 0xffff) && isConnected)
                    enqueCommand<InfoCommand>(SystemConfigPara); // power limit info
            }
            return _commandQueue.front().get()->getCmd();
        }


        void init(void) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:init"));
            initAssignment(&recordMeas, RealTimeRunData_Debug);
            initAssignment(&recordInfo, InverterDevInform_All);
            initAssignment(&recordConfig, SystemConfigPara);
            initAssignment(&recordAlarm, AlarmData);
            toRadioId();
            initialized = true;
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
            }
            return isConnected;
        }

        void clearDevControlRequest() {
            mDevControlRequest = false;
        }

        inline bool getDevControlRequest() {
            return mDevControlRequest;
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
                            rec->record[pos] = (REC_TYP)(val) / (REC_TYP)(div) * generalConfig->yieldEffiency;
                        } else {
                            if ((REC_TYP)(div) > 1)
                                rec->record[pos] = (REC_TYP)(val) / (REC_TYP)(div);
                            else
                                rec->record[pos] = (REC_TYP)(val);
                        }
                    }
                }

                if(rec == &recordMeas) {
                    DPRINTLN(DBG_VERBOSE, "add real time");

                    // get last alarm message index and save it in the inverter object
                    if (getPosByChFld(0, FLD_EVT, rec) == pos) {
                        if (alarmMesIndex < rec->record[pos]) {
                            alarmMesIndex = rec->record[pos];
                            //enqueCommand<InfoCommand>(AlarmUpdate); // What is the function of AlarmUpdate?

                            DPRINT(DBG_INFO, "alarm ID incremented to ");
                            DBGPRINTLN(String(alarmMesIndex));
                            enqueCommand<InfoCommand>(AlarmData);
                        }
                    }
                }
                else if (rec->assign == InfoAssignment) {
                    DPRINTLN(DBG_DEBUG, "add info");
                    // eg. fw version ...
                    isConnected = true;
                }
                else if (rec->assign == SystemConfigParaAssignment) {
                    DPRINTLN(DBG_DEBUG, "add config");
                    if (getPosByChFld(0, FLD_ACT_ACTIVE_PWR_LIMIT, rec) == pos){
                        actPowerLimit = rec->record[pos];
                        DPRINT(DBG_DEBUG, F("Inverter actual power limit: "));
                        DPRINTLN(DBG_DEBUG, String(actPowerLimit, 1));
                    }
                }
                else if (rec->assign == AlarmDataAssignment) {
                    DPRINTLN(DBG_DEBUG, "add alarm");
                    //if (getPosByChFld(0, FLD_LAST_ALARM_CODE, rec) == pos){
                    //    lastAlarmMsg = getAlarmStr(rec->record[pos]);
                    //}
                }
                else
                    DPRINTLN(DBG_WARN, F("add with unknown assginment"));
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

        uint16_t getFwVersion() {
            record_t<> *rec = getRecordStruct(InverterDevInform_All);
            uint8_t pos = getPosByChFld(CH0, FLD_FW_VERSION, rec);
            return getValue(pos, rec);
        }

        uint32_t getLastTs(record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getLastTs"));
            return rec->ts;
        }

        record_t<> *getRecordStruct(uint8_t cmd) {
            switch (cmd) {
                case RealTimeRunData_Debug: return &recordMeas;   // 11 = 0x0b
                case InverterDevInform_All: return &recordInfo;   //  1 = 0x01
                case SystemConfigPara:      return &recordConfig; //  5 = 0x05
                case AlarmData:             return &recordAlarm;  // 17 = 0x11
                default:                    break;
            }
            return NULL;
        }

        void initAssignment(record_t<> *rec, uint8_t cmd) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:initAssignment"));
            rec->ts     = 0;
            rec->length = 0;
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
                        }  /*else if(IV_MI == ivGen) {
                            rec->length  = (uint8_t)(HM1CH_LIST_LEN);
                            rec->assign  = (byteAssign_t *)hm1chAssignment;
                        }*/
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

        uint16_t parseAlarmLog(uint8_t id, uint8_t pyld[], uint8_t len) {
            uint8_t startOff = 2 + id * ALARM_LOG_ENTRY_SIZE;
            if((startOff + ALARM_LOG_ENTRY_SIZE) > len)
                return 0;

            uint16_t wCode = ((uint16_t)pyld[startOff]) << 8 | pyld[startOff+1];
            uint32_t startTimeOffset = 0, endTimeOffset = 0;
            uint32_t start, endTime;

            if (((wCode >> 13) & 0x01) == 1) // check if is AM or PM
                startTimeOffset = 12 * 60 * 60;
            if (((wCode >> 12) & 0x01) == 1) // check if is AM or PM
                endTimeOffset = 12 * 60 * 60;

            start     = (((uint16_t)pyld[startOff + 4] << 8) | ((uint16_t)pyld[startOff + 5])) + startTimeOffset;
            endTime   = (((uint16_t)pyld[startOff + 6] << 8) | ((uint16_t)pyld[startOff + 7])) + endTimeOffset;

            DPRINTLN(DBG_DEBUG, "Alarm #" + String(pyld[startOff+1]) + " '" + String(getAlarmStr(pyld[startOff+1])) + "' start: " + ah::getTimeStr(start) + ", end: " + ah::getTimeStr(endTime));
            addAlarm(pyld[startOff+1], start, endTime);

            alarmCnt++;

            return pyld[startOff+1];
        }

        static String getAlarmStr(uint16_t alarmCode) {
            switch (alarmCode) { // breaks are intentionally missing!
                case 1:    return String(F("Inverter start"));
                case 2:    return String(F("DTU command failed"));
                case 121:  return String(F("Over temperature protection"));
                case 125:  return String(F("Grid configuration parameter error"));
                case 126:  return String(F("Software error code 126"));
                case 127:  return String(F("Firmware error"));
                case 128:  return String(F("Software error code 128"));
                case 129:  return String(F("Software error code 129"));
                case 130:  return String(F("Offline"));
                case 141:  return String(F("Grid overvoltage"));
                case 142:  return String(F("Average grid overvoltage"));
                case 143:  return String(F("Grid undervoltage"));
                case 144:  return String(F("Grid overfrequency"));
                case 145:  return String(F("Grid underfrequency"));
                case 146:  return String(F("Rapid grid frequency change"));
                case 147:  return String(F("Power grid outage"));
                case 148:  return String(F("Grid disconnection"));
                case 149:  return String(F("Island detected"));
                case 205:  return String(F("Input port 1 & 2 overvoltage"));
                case 206:  return String(F("Input port 3 & 4 overvoltage"));
                case 207:  return String(F("Input port 1 & 2 undervoltage"));
                case 208:  return String(F("Input port 3 & 4 undervoltage"));
                case 209:  return String(F("Port 1 no input"));
                case 210:  return String(F("Port 2 no input"));
                case 211:  return String(F("Port 3 no input"));
                case 212:  return String(F("Port 4 no input"));
                case 213:  return String(F("PV-1 & PV-2 abnormal wiring"));
                case 214:  return String(F("PV-3 & PV-4 abnormal wiring"));
                case 215:  return String(F("PV-1 Input overvoltage"));
                case 216:  return String(F("PV-1 Input undervoltage"));
                case 217:  return String(F("PV-2 Input overvoltage"));
                case 218:  return String(F("PV-2 Input undervoltage"));
                case 219:  return String(F("PV-3 Input overvoltage"));
                case 220:  return String(F("PV-3 Input undervoltage"));
                case 221:  return String(F("PV-4 Input overvoltage"));
                case 222:  return String(F("PV-4 Input undervoltage"));
                case 301:  return String(F("Hardware error code 301"));
                case 302:  return String(F("Hardware error code 302"));
                case 303:  return String(F("Hardware error code 303"));
                case 304:  return String(F("Hardware error code 304"));
                case 305:  return String(F("Hardware error code 305"));
                case 306:  return String(F("Hardware error code 306"));
                case 307:  return String(F("Hardware error code 307"));
                case 308:  return String(F("Hardware error code 308"));
                case 309:  return String(F("Hardware error code 309"));
                case 310:  return String(F("Hardware error code 310"));
                case 311:  return String(F("Hardware error code 311"));
                case 312:  return String(F("Hardware error code 312"));
                case 313:  return String(F("Hardware error code 313"));
                case 314:  return String(F("Hardware error code 314"));
                case 5041: return String(F("Error code-04 Port 1"));
                case 5042: return String(F("Error code-04 Port 2"));
                case 5043: return String(F("Error code-04 Port 3"));
                case 5044: return String(F("Error code-04 Port 4"));
                case 5051: return String(F("PV Input 1 Overvoltage/Undervoltage"));
                case 5052: return String(F("PV Input 2 Overvoltage/Undervoltage"));
                case 5053: return String(F("PV Input 3 Overvoltage/Undervoltage"));
                case 5054: return String(F("PV Input 4 Overvoltage/Undervoltage"));
                case 5060: return String(F("Abnormal bias"));
                case 5070: return String(F("Over temperature protection"));
                case 5080: return String(F("Grid Overvoltage/Undervoltage"));
                case 5090: return String(F("Grid Overfrequency/Underfrequency"));
                case 5100: return String(F("Island detected"));
                case 5120: return String(F("EEPROM reading and writing error"));
                case 5150: return String(F("10 min value grid overvoltage"));
                case 5200: return String(F("Firmware error"));
                case 8310: return String(F("Shut down"));
                case 9000: return String(F("Microinverter is suspected of being stolen"));
                default:   return String(F("Unknown"));
            }
        }

    private:
        inline void addAlarm(uint16_t code, uint32_t start, uint32_t end) {
            lastAlarm[alarmNxtWrPos] = alarm_t(code, start, end);
            if(++alarmNxtWrPos >= 10) // rolling buffer
                alarmNxtWrPos = 0;
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

        std::queue<std::shared_ptr<CommandAbstract>> _commandQueue;
        bool          mDevControlRequest; // true if change needed
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
        if(dcPower > dcMaxPower)
            return dcPower;
    }
    return dcMaxPower;
}

#endif /*__HM_INVERTER_H__*/
