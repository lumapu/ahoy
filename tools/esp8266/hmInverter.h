//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HM_INVERTER_H__
#define __HM_INVERTER_H__

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "hmDefines.h"
#include <memory>
#include <queue>

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
    { CALC_YT_CH0,  &calcYieldTotalCh0 },
    { CALC_YD_CH0,  &calcYieldDayCh0   },
    { CALC_UDC_CH,  &calcUdcCh         },
    { CALC_PDC_CH0, &calcPowerDcCh0    },
    { CALC_EFF_CH0, &calcEffiencyCh0   },
    { CALC_IRR_CH,  &calcIrradiation   }
};


template <class REC_TYP>
class Inverter {
    public:
        uint8_t       id;                    // unique id
        char          name[MAX_NAME_LENGTH]; // human readable name, eg. "HM-600.1"
        uint8_t       type;                  // integer which refers to inverter type
        uint16_t      alarmMesIndex;         // Last recorded Alarm Message Index
        uint16_t      fwVersion;             // Firmware Version from Info Command Request
        uint16_t      powerLimit[2];         // limit power output
        float         actPowerLimit;         // actual power limit
        uint8_t       devControlCmd;         // carries the requested cmd
        bool          devControlRequest;     // true if change needed
        serial_u      serial;                // serial number as on barcode
        serial_u      radioId;               // id converted to modbus
        uint8_t       channels;              // number of PV channels (1-4)
        record_t<REC_TYP> recordMeas;        // structure for measured values
        record_t<REC_TYP> recordInfo;        // structure for info values
        record_t<REC_TYP> recordConfig;      // structure for system config values
        record_t<REC_TYP> recordAlarm;       // structure for alarm values
        uint16_t      chMaxPwr[4];           // maximum power of the modules (Wp)
        char          chName[4][MAX_NAME_LENGTH]; // human readable name for channels
        String        lastAlarmMsg;
        bool          initialized;           // needed to check if the inverter was correctly added (ESP32 specific - union types are never null)

        Inverter() {
            powerLimit[0] = 0xffff;       // 65535 W Limit -> unlimited
            powerLimit[1] = NoPowerLimit; // default power limit setting
            actPowerLimit = 0xffff;       // init feedback from inverter to -1
            devControlRequest = false;
            devControlCmd = InitDataState;
            initialized = false;
            fwVersion = 0;
            lastAlarmMsg =  "nothing";
            alarmMesIndex = 0;
        }

        ~Inverter() {
            // TODO: cleanup
        }

        template <typename T>
        void enqueCommand(uint8_t cmd) {
           _commandQueue.push(std::make_shared<T>(cmd));
           DPRINTLN(DBG_INFO, "enqueuedCmd: " + String(cmd));
        }

        void setQueuedCmdFinished() {
            if (!_commandQueue.empty()) {
                // Will destroy CommandAbstract Class Object (?)
                _commandQueue.pop(); 
            }
        }

        void clearCmdQueue() {
            while (!_commandQueue.empty()) {
                // Will destroy CommandAbstract Class Object (?)
                _commandQueue.pop(); 
            }
        }

    	uint8_t getQueuedCmd() {
            if (_commandQueue.empty()){
                // Fill with default commands
                enqueCommand<InfoCommand>(RealTimeRunData_Debug);
                if (fwVersion == 0)
                { // info needed maybe after "one night" (=> DC>0 to DC=0 and to DC>0) or reboot
                    enqueCommand<InfoCommand>(InverterDevInform_All);
                }
                if (actPowerLimit == 0xffff)
                { // info needed maybe after "one nigth" (=> DC>0 to DC=0 and to DC>0) or reboot
                    enqueCommand<InfoCommand>(SystemConfigPara);
                }
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
            memset(name, 0, MAX_NAME_LENGTH);
            memset(chName, 0, MAX_NAME_LENGTH * 4);
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
                        if ((REC_TYP)(div) > 1)
                            rec->record[pos] = (REC_TYP)(val) / (REC_TYP)(div);
                        else
                            rec->record[pos] = (REC_TYP)(val);
                    }
                }

                if(rec == &recordMeas) {
                    DPRINTLN(DBG_VERBOSE, "add real time");

                    // get last alarm message index and save it in the inverter object
                    if (getPosByChFld(0, FLD_EVT, rec) == pos){
                        if (alarmMesIndex < rec->record[pos]){
                            alarmMesIndex = rec->record[pos];
                            //enqueCommand<InfoCommand>(AlarmUpdate); // What is the function of AlarmUpdate?
                            enqueCommand<InfoCommand>(AlarmData);
                        }
                        else {
                            alarmMesIndex = rec->record[pos]; // no change
                        }
                    }
                }
                else if (rec->assign == InfoAssignment) {
                    DPRINTLN(DBG_DEBUG, "add info");
                    // get at least the firmware version and save it to the inverter object
                    if (getPosByChFld(0, FLD_FW_VERSION, rec) == pos){
                        fwVersion = rec->record[pos];
                        DPRINT(DBG_DEBUG, F("Inverter FW-Version: ") + String(fwVersion));
                    }
                }
                else if (rec->assign == SystemConfigParaAssignment) {
                    DPRINTLN(DBG_DEBUG, "add config");
                    // get at least the firmware version and save it to the inverter object
                    if (getPosByChFld(0, FLD_ACT_ACTIVE_PWR_LIMIT, rec) == pos){
                        actPowerLimit = rec->record[pos];
                        DPRINT(DBG_DEBUG, F("Inverter actual power limit: ") + String(actPowerLimit, 1));
                    }
                }
                else if (rec->assign == AlarmDataAssignment) {
                    DPRINTLN(DBG_DEBUG, "add alarm");
                    if (getPosByChFld(0, FLD_LAST_ALARM_CODE, rec) == pos){
                        lastAlarmMsg = getAlarmStr(rec->record[pos]);
                    }
                }
                else
                    DPRINTLN(DBG_WARN, F("add with unknown assginment"));
            }
            else
                DPRINTLN(DBG_ERROR, F("addValue: assignment not found with cmd 0x"));
        }

        REC_TYP getValue(uint8_t pos, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getValue"));
            if(NULL == rec)
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

        bool isAvailable(uint32_t timestamp, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:isAvailable"));
            return ((timestamp - rec->ts) < INACT_THRES_SEC);
        }

        bool isProducing(uint32_t timestamp, record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:isProducing"));
            if(isAvailable(timestamp, rec)) {
                uint8_t pos = getPosByChFld(CH0, FLD_PAC, rec);
                return (getValue(pos, rec) > INACT_PWR_THRESH);
            }
            return false;
        }

        uint32_t getLastTs(record_t<> *rec) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getLastTs"));
            return rec->ts;
        }

        record_t<> *getRecordStruct(uint8_t cmd) {
            switch (cmd) {
                case RealTimeRunData_Debug: return &recordMeas;
                case InverterDevInform_All: return &recordInfo;
                case SystemConfigPara:      return &recordConfig;
                case AlarmData:             return &recordAlarm;
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
                        rec->length = (uint8_t)(HM1CH_LIST_LEN);
                        rec->assign = (byteAssign_t *)hm1chAssignment;
                        channels    = 1;
                    }
                    else if (INV_TYPE_2CH == type) {
                        rec->length = (uint8_t)(HM2CH_LIST_LEN);
                        rec->assign = (byteAssign_t *)hm2chAssignment;
                        channels    = 2;
                    }
                    else if (INV_TYPE_4CH == type) {
                        rec->length = (uint8_t)(HM4CH_LIST_LEN);
                        rec->assign = (byteAssign_t *)hm4chAssignment;
                        channels    = 4;
                    }
                    else {
                        rec->length = 0;
                        rec->assign = NULL;
                        channels    = 0;
                    }
                    break;
                case InverterDevInform_All:
                    rec->length = (uint8_t)(HMINFO_LIST_LEN);
                    rec->assign = (byteAssign_t *)InfoAssignment;
                    break;
                case SystemConfigPara:
                    rec->length = (uint8_t)(HMSYSTEM_LIST_LEN);
                    rec->assign = (byteAssign_t *)SystemConfigParaAssignment;
                    break;
                case AlarmData:
                    rec->length = (uint8_t)(HMALARMDATA_LIST_LEN);
                    rec->assign = (byteAssign_t *)AlarmDataAssignment;
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

        String getAlarmStr(u_int16_t alarmCode) {
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
        std::queue<std::shared_ptr<CommandAbstract>> _commandQueue;
        void toRadioId(void) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:toRadioId"));
            radioId.u64  = 0ULL;
            radioId.b[4] = serial.b[0];
            radioId.b[3] = serial.b[1];
            radioId.b[2] = serial.b[2];
            radioId.b[1] = serial.b[3];
            radioId.b[0] = 0x01;
        }
};


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
            uint8_t pos = iv->getPosByChFld(i, FLD_YT, rec);
            yield += iv->getValue(pos, rec);
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
            uint8_t pos = iv->getPosByChFld(i, FLD_YD, rec);
            yield += iv->getValue(pos, rec);
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
            uint8_t pos = iv->getPosByChFld(i, FLD_PDC, rec);
            dcPower += iv->getValue(pos, rec);
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
        uint8_t pos = iv->getPosByChFld(CH0, FLD_PAC, rec);
        T acPower = iv->getValue(pos, rec);
        T dcPower = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            pos = iv->getPosByChFld(i, FLD_PDC, rec);
            dcPower += iv->getValue(pos, rec);
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
        uint8_t pos = iv->getPosByChFld(arg0, FLD_PDC, rec);
        if(iv->chMaxPwr[arg0-1] > 0)
            return iv->getValue(pos, rec) / iv->chMaxPwr[arg0-1] * 100.0f;
    }
    return 0.0;
}

#endif /*__HM_INVERTER_H__*/
