//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __DEFINES_H__
#define __DEFINES_H__

#include "config/config.h"

//-------------------------------------
// VERSION
//-------------------------------------
#define VERSION_MAJOR       0
#define VERSION_MINOR       7
#define VERSION_PATCH       36

//-------------------------------------
typedef struct {
    uint8_t ch;
    uint8_t len;
    uint8_t packet[MAX_RF_PAYLOAD_SIZE];
} packet_t;

typedef enum {
    InverterDevInform_Simple = 0,  // 0x00
    InverterDevInform_All    = 1,  // 0x01
    GridOnProFilePara        = 2,  // 0x02
    HardWareConfig           = 3,  // 0x03
    SimpleCalibrationPara    = 4,  // 0x04
    SystemConfigPara         = 5,  // 0x05
    RealTimeRunData_Debug    = 11, // 0x0b
    RealTimeRunData_Reality  = 12, // 0x0c
    RealTimeRunData_A_Phase  = 13, // 0x0d
    RealTimeRunData_B_Phase  = 14, // 0x0e
    RealTimeRunData_C_Phase  = 15, // 0x0f
    AlarmData                = 17, // 0x11, Alarm data - all unsent alarms
    AlarmUpdate              = 18, // 0x12, Alarm data - all pending alarms
    RecordData               = 19, // 0x13
    InternalData             = 20, // 0x14
    GetLossRate              = 21, // 0x15
    GetSelfCheckState        = 30, // 0x1e
    InitDataState            = 0xff
} InfoCmdType;

typedef enum {
    TurnOn                  = 0,  // 0x00
    TurnOff                 = 1,  // 0x01
    Restart                 = 2,  // 0x02
    Lock                    = 3,  // 0x03
    Unlock                  = 4,  // 0x04
    ActivePowerContr        = 11, // 0x0b
    ReactivePowerContr      = 12, // 0x0c
    PFSet                   = 13, // 0x0d
    CleanState_LockAndAlarm = 20, // 0x14
    SelfInspection          = 40, // 0x28, self-inspection of grid-connected protection files
    Init                    = 0xff
} DevControlCmdType;

typedef enum {
    AbsolutNonPersistent  = 0UL,    // 0x0000
    RelativNonPersistent  = 1UL,    // 0x0001
    AbsolutPersistent     = 256UL,  // 0x0100
    RelativPersistent     = 257UL   // 0x0101
} PowerLimitControlType;

union serial_u {
    uint64_t u64;
    uint8_t  b[8];
};

#define MIN_SERIAL_INTERVAL     2 // 5
#define MIN_SEND_INTERVAL       15
#define MIN_MQTT_INTERVAL       60


enum {MQTT_STATUS_OFFLINE = 0, MQTT_STATUS_PARTIAL, MQTT_STATUS_ONLINE};

//-------------------------------------
// EEPROM
//-------------------------------------
#define SSID_LEN                32
#define PWD_LEN                 64
#define DEVNAME_LEN             16
#define NTP_ADDR_LEN            32 // DNS Name

#define MQTT_ADDR_LEN           64 // DNS Name
#define MQTT_CLIENTID_LEN       22 // number of chars is limited to 23 up to v3.1 of MQTT
#define MQTT_USER_LEN           65 // there is another byte necessary for \0
#define MQTT_PWD_LEN            65
#define MQTT_TOPIC_LEN          65

#define MQTT_MAX_PACKET_SIZE    384

typedef struct {
    uint32_t rxFail;
    uint32_t rxFailNoAnser;
    uint32_t rxSuccess;
    uint32_t frmCnt;
} statistics_t;

#endif /*__DEFINES_H__*/
