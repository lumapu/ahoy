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
template <class RECORDTYPE=float>
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


class CommandAbstract {
    public:
        CommandAbstract(uint8_t txType = 0, uint8_t cmd = 0){
            _TxType = txType;
            _Cmd = cmd;
        };
        virtual ~CommandAbstract() {};

        const uint8_t getCmd()
        {       
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


template <class RECORDTYPE>
class Inverter {
    public:
        uint8_t       id;       // unique id
        char          name[MAX_NAME_LENGTH]; // human readable name, eg. "HM-600.1"
        uint8_t       type;     // integer which refers to inverter type
        byteAssign_t* assign;   // type of inverter
        uint8_t       listLen;  // length of assignments
        uint16_t      alarmMesIndex; // Last recorded Alarm Message Index
        uint16_t      fwVersion; // Firmware Version from Info Command Request
        uint16_t      powerLimit[2];  // limit power output
        uint16_t      actPowerLimit; //
        uint8_t       devControlCmd;  // carries the requested cmd
        bool          devControlRequest; // true if change needed
        serial_u      serial;   // serial number as on barcode
        serial_u      radioId;  // id converted to modbus
        uint8_t       channels; // number of PV channels (1-4)
        uint32_t      ts;       // timestamp of last received payload
        RECORDTYPE    *record;  // pointer for values
        uint16_t      chMaxPwr[4]; // maximum power of the modules (Wp)
        char          chName[4][MAX_NAME_LENGTH]; // human readable name for channel
        String        lastAlarmMsg;
        bool          initialized; // needed to check if the inverter was correctly added (ESP32 specific - union types are never null)

        Inverter() {
            ts = 0;
            powerLimit[0] = 0xffff; // 65535 W Limit -> unlimited
            powerLimit[1] = 0x0000; // 
            actPowerLimit = 0xffff; // init feedback from inverter to -1
            devControlRequest = false;
            devControlCmd = 0xff;
            initialized = false;
            fwVersion = 0;
            lastAlarmMsg =  "nothing";
            alarmMesIndex = 0;
        }

        ~Inverter() {
            // TODO: cleanup
        }

        template <typename T>
        void enqueCommand(uint8_t cmd)
        {
           _commandQueue.push(std::make_shared<T>(cmd));
           DPRINTLN(DBG_INFO, "enqueuedCmd: " + String(cmd));
        }

        void setQueuedCmdFinished(){
            if (!_commandQueue.empty()){
                // Will destroy CommandAbstract Class Object (?)
                _commandQueue.pop(); 
            }
        }

    	uint8_t getQueuedCmd()
        {
            if (_commandQueue.empty()){
                // Fill with default commands
                enqueCommand<InfoCommand>(RealTimeRunData_Debug);
                if (fwVersion == 0)
                { // info needed maybe after "one nigth" (=> DC>0 to DC=0 and to DC>0) or reboot
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
            getAssignment();
            toRadioId();
            record = new RECORDTYPE[listLen];
            memset(name, 0, MAX_NAME_LENGTH);
            memset(chName, 0, MAX_NAME_LENGTH * 4);
            memset(record, 0, sizeof(RECORDTYPE) * listLen);
            initialized = true;
        }

        uint8_t getPosByChFld(uint8_t channel, uint8_t fieldId) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getPosByChFld"));
            uint8_t pos = 0;
            for(; pos < listLen; pos++) {
                if((assign[pos].ch == channel) && (assign[pos].fieldId == fieldId))
                    break;
            }
            return (pos >= listLen) ? 0xff : pos;
        }

        const char *getFieldName(uint8_t pos) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getFieldName"));
            return fields[assign[pos].fieldId];
        }

        const char *getUnit(uint8_t pos) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getUnit"));
            return units[assign[pos].unitId];
        }

        uint8_t getChannel(uint8_t pos) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getChannel"));
            return assign[pos].ch;
        }

        void addValue(uint8_t pos, uint8_t buf[]) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:addValue"));
            uint8_t cmd = getQueuedCmd();
            uint8_t ptr  = assign[pos].start;
            uint8_t end  = ptr + assign[pos].num;
            uint16_t div = assign[pos].div;
            if(CMD_CALC != div) {
                uint32_t val = 0;
                do {
                    val <<= 8;
                    val |= buf[ptr];
                } while(++ptr != end);
                if ((RECORDTYPE)(div) > 1){
                    record[pos] = (RECORDTYPE)(val) / (RECORDTYPE)(div);                
                }
                else {
                    record[pos] = (RECORDTYPE)(val);                
                }
                
            }
            if (cmd == RealTimeRunData_Debug) {
                // get last alarm message index and save it in the inverter object
                if (getPosByChFld(0, FLD_ALARM_MES_ID) == pos){ 
                    if (alarmMesIndex < record[pos]){
                        alarmMesIndex = record[pos];
                        //enqueCommand<InfoCommand>(AlarmUpdate); // What is the function of AlarmUpdate?
                        enqueCommand<InfoCommand>(AlarmData);
                    }
                    else {
                        alarmMesIndex = record[pos]; // no change
                    }
                }
            }
            if (cmd == InverterDevInform_All) {
                // get at least the firmware version and save it to the inverter object
                if (getPosByChFld(0, FLD_FW_VERSION) == pos){ 
                    fwVersion = record[pos];
                    DPRINT(DBG_DEBUG, F("Inverter FW-Version: ") + String(fwVersion));
                }
            }
            if (cmd == SystemConfigPara) {
                // get at least the firmware version and save it to the inverter object
                if (getPosByChFld(0, FLD_ACT_PWR_LIMIT) == pos){ 
                    actPowerLimit = record[pos];
                    DPRINT(DBG_DEBUG, F("Inverter actual power limit: ") + String(actPowerLimit));
                }
            }
            if (cmd == AlarmData){
                if (getPosByChFld(0, FLD_LAST_ALARM_CODE) == pos){ 
                    lastAlarmMsg = getAlarmStr(record[pos]);
                }
            }
        }

        RECORDTYPE getValue(uint8_t pos) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getValue"));
            return record[pos];
        }

        void doCalculations() {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:doCalculations"));
            uint8_t cmd = getQueuedCmd();
            getAssignment();
            if (cmd == RealTimeRunData_Debug){
                for(uint8_t i = 0; i < listLen; i++) {
                    if(CMD_CALC == assign[i].div) {
                        record[i] = calcFunctions<RECORDTYPE>[assign[i].start].func(this, assign[i].num);
                    }
                    yield();
                }
            }
        }

        bool isAvailable(uint32_t timestamp) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:isAvailable"));
            return ((timestamp - ts) < INACT_THRES_SEC);
        }

        bool isProducing(uint32_t timestamp) {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:isProducing"));
            if(isAvailable(timestamp)) {
                uint8_t pos = getPosByChFld(CH0, FLD_PAC);
                return (getValue(pos) > INACT_PWR_THRESH);
            }
            return false;
        }

        uint32_t getLastTs(void)
        {
            DPRINTLN(DBG_VERBOSE, F("hmInverter.h:getLastTs"));
            return ts;
        }

        void getAssignment()
        {
            DPRINTLN(DBG_DEBUG, F("hmInverter.h:getAssignment"));
            // Default assignment;
            if (INV_TYPE_1CH == type)
            {
                listLen = (uint8_t)(HM1CH_LIST_LEN);
                assign = (byteAssign_t *)hm1chAssignment;
                channels = 1;
            }
            else if (INV_TYPE_2CH == type)
            {
                listLen = (uint8_t)(HM2CH_LIST_LEN);
                assign = (byteAssign_t *)hm2chAssignment;
                channels = 2;
            }
            else if (INV_TYPE_4CH == type)
            {
                listLen = (uint8_t)(HM4CH_LIST_LEN);
                assign = (byteAssign_t *)hm4chAssignment;
                channels = 4;
            }
            else
            {
                listLen = 0;
                channels = 0;
                assign = NULL;
            }

            uint8_t cmd = getQueuedCmd();
            switch (cmd)
            {
            case RealTimeRunData_Debug:
                // Do nothing will use default
                break;
            case InverterDevInform_All:
                listLen = (uint8_t)(HMINFO_LIST_LEN);
                assign = (byteAssign_t *)InfoAssignment;
                break;
            case SystemConfigPara:
                listLen = (uint8_t)(HMSYSTEM_LIST_LEN);
                assign = (byteAssign_t *)SystemConfigParaAssignment;
                break;
            case AlarmData:
                listLen = (uint8_t)(HMALARMDATA_LIST_LEN);
                assign = (byteAssign_t *)AlarmDataAssignment;
                break;
            default:
                DPRINTLN(DBG_INFO, "Parser not implemented");
            }
        }
        String getAlarmStr(u_int16_t alarmCode)
        {
            switch (alarmCode)
            {
            case 1:
                return String(F("Inverter start"));
                break;
            case 2:
                return String(F("DTU command failed"));
                break;
            case 121:
                return String(F("Over temperature protection"));
                break;
            case 125:
                return String(F("Grid configuration parameter error"));
                break;
            case 126:
                return String(F("Software error code 126"));
                break;
            case 127:
                return String(F("Firmware error"));
                break;
            case 128:
                return String(F("Software error code 128"));
                break;
            case 129:
                return String(F("Software error code 129"));
                break;
            case 130:
                return String(F("Offline"));
                break;
            case 141:
                return String(F("Grid overvoltage"));
                break;
            case 142:
                return String(F("Average grid overvoltage"));
                break;
            case 143:
                return String(F("Grid undervoltage"));
                break;
            case 144:
                return String(F("Grid overfrequency"));
                break;
            case 145:
                return String(F("Grid underfrequency"));
                break;
            case 146:
                return String(F("Rapid grid frequency change"));
                break;
            case 147:
                return String(F("Power grid outage"));
                break;
            case 148:
                return String(F("Grid disconnection"));
                break;
            case 149:
                return String(F("Island detected"));
                break;
            case 205:
                return String(F("Input port 1 & 2 overvoltage"));
                break;
            case 206:
                return String(F("Input port 3 & 4 overvoltage"));
                break;
            case 207:
                return String(F("Input port 1 & 2 undervoltage"));
                break;
            case 208:
                return String(F("Input port 3 & 4 undervoltage"));
                break;
            case 209:
                return String(F("Port 1 no input"));
                break;
            case 210:
                return String(F("Port 2 no input"));
                break;
            case 211:
                return String(F("Port 3 no input"));
                break;
            case 212:
                return String(F("Port 4 no input"));
                break;
            case 213:
                return String(F("PV-1 & PV-2 abnormal wiring"));
                break;
            case 214:
                return String(F("PV-3 & PV-4 abnormal wiring"));
                break;
            case 215:
                return String(F("PV-1 Input overvoltage"));
                break;
            case 216:
                return String(F("PV-1 Input undervoltage"));
                break;
            case 217:
                return String(F("PV-2 Input overvoltage"));
                break;
            case 218:
                return String(F("PV-2 Input undervoltage"));
                break;
            case 219:
                return String(F("PV-3 Input overvoltage"));
                break;
            case 220:
                return String(F("PV-3 Input undervoltage"));
                break;
            case 221:
                return String(F("PV-4 Input overvoltage"));
                break;
            case 222:
                return String(F("PV-4 Input undervoltage"));
                break;
            case 301:
                return String(F("Hardware error code 301"));
                break;
            case 302:
                return String(F("Hardware error code 302"));
                break;
            case 303:
                return String(F("Hardware error code 303"));
                break;
            case 304:
                return String(F("Hardware error code 304"));
                break;
            case 305:
                return String(F("Hardware error code 305"));
                break;
            case 306:
                return String(F("Hardware error code 306"));
                break;
            case 307:
                return String(F("Hardware error code 307"));
                break;
            case 308:
                return String(F("Hardware error code 308"));
                break;
            case 309:
                return String(F("Hardware error code 309"));
                break;
            case 310:
                return String(F("Hardware error code 310"));
                break;
            case 311:
                return String(F("Hardware error code 311"));
                break;
            case 312:
                return String(F("Hardware error code 312"));
                break;
            case 313:
                return String(F("Hardware error code 313"));
                break;
            case 314:
                return String(F("Hardware error code 314"));
                break;
            case 5041:
                return String(F("Error code-04 Port 1"));
                break;
            case 5042:
                return String(F("Error code-04 Port 2"));
                break;
            case 5043:
                return String(F("Error code-04 Port 3"));
                break;
            case 5044:
                return String(F("Error code-04 Port 4"));
                break;
            case 5051:
                return String(F("PV Input 1 Overvoltage/Undervoltage"));
                break;
            case 5052:
                return String(F("PV Input 2 Overvoltage/Undervoltage"));
                break;
            case 5053:
                return String(F("PV Input 3 Overvoltage/Undervoltage"));
                break;
            case 5054:
                return String(F("PV Input 4 Overvoltage/Undervoltage"));
                break;
            case 5060:
                return String(F("Abnormal bias"));
                break;
            case 5070:
                return String(F("Over temperature protection"));
                break;
            case 5080:
                return String(F("Grid Overvoltage/Undervoltage"));
                break;
            case 5090:
                return String(F("Grid Overfrequency/Underfrequency"));
                break;
            case 5100:
                return String(F("Island detected"));
                break;
            case 5120:
                return String(F("EEPROM reading and writing error"));
                break;
            case 5150:
                return String(F("10 min value grid overvoltage"));
                break;
            case 5200:
                return String(F("Firmware error"));
                break;
            case 8310:
                return String(F("Shut down"));
                break;
            case 9000:
                return String(F("Microinverter is suspected of being stolen"));
                break;
            default:
                return String(F("Unknown"));
                break;
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
        T yield = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            uint8_t pos = iv->getPosByChFld(i, FLD_YT);
            yield += iv->getValue(pos);
        }
        return yield;
    }
    return 0.0;
}

template<class T=float>
static T calcYieldDayCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcYieldDayCh0"));
    if(NULL != iv) {
        T yield = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            uint8_t pos = iv->getPosByChFld(i, FLD_YD);
            yield += iv->getValue(pos);
        }
        return yield;
    }
    return 0.0;
}

template<class T=float>
static T calcUdcCh(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcUdcCh"));
    // arg0 = channel of source
    for(uint8_t i = 0; i < iv->listLen; i++) {
        if((FLD_UDC == iv->assign[i].fieldId) && (arg0 == iv->assign[i].ch)) {
            return iv->getValue(i);
        }
    }

    return 0.0;
}

template<class T=float>
static T calcPowerDcCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcPowerDcCh0"));
    if(NULL != iv) {
        T dcPower = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            uint8_t pos = iv->getPosByChFld(i, FLD_PDC);
            dcPower += iv->getValue(pos);
        }
        return dcPower;
    }
    return 0.0;
}

template<class T=float>
static T calcEffiencyCh0(Inverter<> *iv, uint8_t arg0) {
    DPRINTLN(DBG_VERBOSE, F("hmInverter.h:calcEfficiencyCh0"));
    if(NULL != iv) {
        uint8_t pos = iv->getPosByChFld(CH0, FLD_PAC);
        T acPower = iv->getValue(pos);
        T dcPower = 0;
        for(uint8_t i = 1; i <= iv->channels; i++) {
            pos = iv->getPosByChFld(i, FLD_PDC);
            dcPower += iv->getValue(pos);
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
        uint8_t pos = iv->getPosByChFld(arg0, FLD_PDC);
        if(iv->chMaxPwr[arg0-1] > 0)
            return iv->getValue(pos) / iv->chMaxPwr[arg0-1] * 100.0f;
    }
    return 0.0;
}

#endif /*__HM_INVERTER_H__*/
