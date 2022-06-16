//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __CRC_H__
#define __CRC_H__

#include <cstdint>
#include "Arduino.h"

#define CRC8_INIT               0x00
#define CRC8_POLY               0x01

#define CRC16_MODBUS_POLYNOM    0xA001

uint8_t crc8(uint8_t buf[], uint8_t len);
uint16_t crc16(uint8_t buf[], uint8_t len, uint16_t start = 0xffff);

#endif /*__CRC_H__*/
