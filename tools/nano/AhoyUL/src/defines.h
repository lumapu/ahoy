//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

//2022: mb some adaptation for Arduinio Nano version

#ifndef __DEFINES_H__
#define __DEFINES_H__

#include "config.h"

//-------------------------------------
// VERSION
//-------------------------------------
#define VERSION_MAJOR       0
#define VERSION_MINOR       5
#define VERSION_PATCH       17+


//-------------------------------------
typedef struct {
    uint8_t rfch;
    uint8_t plen;
    uint8_t data[MAX_RF_PAYLOAD_SIZE];
} packet_t;


typedef struct {
    uint8_t txId;
    uint8_t invId[5];                                                                   // 5byte inverter address field is used
    uint16_t invType;                                                                   // 2bytes inverter type header e.g. 0x1141 for hoymiles 2 channel type                                                               
    volatile uint32_t ts;                                                                //timestamp in millisec
    uint8_t data[MAX_PAYLOAD_ENTRIES][MAX_RF_PAYLOAD_SIZE];
    uint8_t len[MAX_PAYLOAD_ENTRIES];
    bool receive;                      //indicates that at least one packet was received and stored --> trigger retransmission of missing packets                                      
    uint8_t maxPackId;                 //indicates the last fragment id was received and defines the end of payload data packet
    uint8_t retransmits;
    bool requested;                    //indicates the sending requst
    uint8_t rxChIdx;                   //mb 2022-10-30: added to keep the last rx channel index for the evaluated payload. I think its needed to handle different inverter simultaniously, which can work on different frequencies.
    bool isMACPacket;
} invPayload_t;


typedef enum {
    InverterDevInform_Simple = 0,   // 0x00
    InverterDevInform_All = 1,      // 0x01
    GridOnProFilePara = 2,        // 0x02
    HardWareConfig = 3,           // 0x03
    SimpleCalibrationPara = 4,    // 0x04
    SystemConfigPara = 5,           // 0x05
    RealTimeRunData_Debug = 11,     // 0x0b
    RealTimeRunData_Reality = 12, // 0x0c
    RealTimeRunData_A_Phase = 13, // 0x0d
    RealTimeRunData_B_Phase = 14, // 0x0e
    RealTimeRunData_C_Phase = 15, // 0x0f
    AlarmData = 17,                 // 0x11, Alarm data - all unsent alarms
    AlarmUpdate = 18,               // 0x12, Alarm data - all pending alarms
    RecordData = 19,              // 0x13
    InternalData = 20,            // 0x14
    GetLossRate = 21,               // 0x15
    GetSelfCheckState = 30,       // 0x1e
    InitDataState = 0xff
} InfoCmdType;

typedef enum {
    TurnOn = 0,                   // 0x00
    TurnOff = 1,                  // 0x01
    Restart = 2,                  // 0x02
    Lock = 3,                     // 0x03
    Unlock = 4,                   // 0x04
    ActivePowerContr = 11,        // 0x0b
    ReactivePowerContr = 12,      // 0x0c
    PFSet = 13,                   // 0x0d
    CleanState_LockAndAlarm = 20, // 0x14
    SelfInspection = 40,          // 0x28, self-inspection of grid-connected protection files
    Init = 0xff
} DevControlCmdType;

typedef enum { // ToDo: to be verified by field tests
    NoPowerLimit            = 0xffff, // ahoy internal value, no hoymiles value!
    AbsolutNonPersistent    = 0UL,  // 0x0000
    RelativNonPersistent    = 1UL,  // 0x0001
    AbsolutPersistent       = 256UL, // 0x0100
    RelativPersistent       = 257UL // 0x0101
} PowerLimitControlType;


// minimum serial interval
#define MIN_SERIAL_INTERVAL     5

// minimum send interval
#define MIN_SEND_INTERVAL       15

#define INV_ADDR_LEN            MAX_NUM_INVERTERS * 8                   // uint64_t
#define INV_NAME_LEN            MAX_NUM_INVERTERS * MAX_NAME_LENGTH     // char[]
#define INV_CH_CH_PWR_LEN       MAX_NUM_INVERTERS * 2 * 4               // uint16_t (4 channels)
#define INV_CH_CH_NAME_LEN      MAX_NUM_INVERTERS * MAX_NAME_LENGTH * 4 // (4 channels)
#define INV_INTERVAL_LEN        2                                       // uint16_t
#define INV_MAX_RTRY_LEN        1                                       // uint8_t
#define INV_PWR_LIM_LEN         MAX_NUM_INVERTERS * 2                   // uint16_t

#define PINOUT_LEN              3 // 3 pins: CS, CE, IRQ
#define RF24_AMP_PWR_LEN        1

#define SER_ENABLE_LEN          1 // uint8_t
#define SER_DEBUG_LEN           1 // uint8_t
#define SER_INTERVAL_LEN        2 // uint16_t

typedef struct {
    // nrf24
    uint16_t sendInterval;
    uint8_t maxRetransPerPyld;
    uint8_t pinCs;
    uint8_t pinCe;
    uint8_t pinIrq;
    uint8_t amplifierPower;

    // serial
    uint16_t serialInterval;
    bool serialShowIv;
    bool serialDebug;
} /*__attribute__((__packed__))*/ config_t;


#define ADDR_CFG                ADDR_START_SETTINGS
#define ADDR_CFG_INVERTER       ADDR_CFG + CFG_LEN

#define ADDR_INV_ADDR           ADDR_CFG_INVERTER
#define ADDR_INV_NAME           ADDR_INV_ADDR      + INV_ADDR_LEN
#define ADDR_INV_CH_PWR         ADDR_INV_NAME      + INV_NAME_LEN
#define ADDR_INV_CH_NAME        ADDR_INV_CH_PWR    + INV_CH_CH_PWR_LEN
#define ADDR_INV_INTERVAL       ADDR_INV_CH_NAME   + INV_CH_CH_NAME_LEN
#define ADDR_INV_MAX_RTRY       ADDR_INV_INTERVAL  + INV_INTERVAL_LEN
#define ADDR_INV_PWR_LIM        ADDR_INV_MAX_RTRY  + INV_MAX_RTRY_LEN
#define ADDR_INV_PWR_LIM_CON    ADDR_INV_PWR_LIM   + INV_PWR_LIM_LEN

#define ADDR_NEXT               ADDR_INV_PWR_LIM_CON   + INV_PWR_LIM_LEN


#define ADDR_SETTINGS_CRC       ADDR_NEXT + 2

#if(ADDR_SETTINGS_CRC <= ADDR_NEXT)
#pragma error "address overlap! (ADDR_SETTINGS_CRC="+ ADDR_SETTINGS_CRC +", ADDR_NEXT="+ ADDR_NEXT +")"
#endif

#if(ADDR_SETTINGS_CRC >= 4096 - CRC_LEN)
#pragma error "EEPROM size exceeded! (ADDR_SETTINGS_CRC="+ ADDR_SETTINGS_CRC +", CRC_LEN="+ CRC_LEN +")"
#pragma error "Configure less inverters? (MAX_NUM_INVERTERS=" + MAX_NUM_INVERTERS +")"
#endif



#endif /*__DEFINES_H__*/
