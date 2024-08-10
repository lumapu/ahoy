//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __DEFINES_H__
#define __DEFINES_H__

#include "config/config.h"

//-------------------------------------
// VERSION
//-------------------------------------
#define VERSION_MAJOR       0
#define VERSION_MINOR       8
#define VERSION_PATCH       134
//-------------------------------------
typedef struct {
    uint8_t ch;
    uint8_t len;
    int8_t rssi;
    uint8_t packet[MAX_RF_PAYLOAD_SIZE];
    uint16_t millis;
} packet_t;

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

enum {
    DISP_TYPE_T0_NONE           = 0,
    DISP_TYPE_T1_SSD1306_128X64 = 1,
    DISP_TYPE_T2_SH1106_128X64  = 2,
    DISP_TYPE_T3_PCD8544_84X48  = 3,
    DISP_TYPE_T4_SSD1306_128X32 = 4,
    DISP_TYPE_T5_SSD1306_64X48  = 5,
    DISP_TYPE_T6_SSD1309_128X64 = 6,
    DISP_TYPE_T10_EPAPER        = 10
};

typedef struct {
    uint32_t rxFail;
    uint32_t rxFailNoAnswer;
    uint32_t rxSuccess;
    uint32_t frmCnt;
    uint32_t txCnt;
    uint32_t retransmits;
    uint16_t ivLoss;   // lost frames (from GetLossRate)
    uint16_t ivSent;   // sent frames (from GetLossRate)
    uint16_t dtuLoss;  // current DTU lost frames (since last GetLossRate)
    uint16_t dtuSent;  // current DTU sent frames (since last GetLossRate)
} statistics_t;

#endif /*__DEFINES_H__*/
