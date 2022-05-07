

#define BITS_TO_BYTES(x)  (((x)+7)>>3)
#define BYTES_TO_BITS(x)  ((x)<<3)

extern uint16_t crc16_modbus(uint8_t *puchMsg, uint16_t usDataLen);
extern uint8_t crc8(uint8_t *buf, const uint16_t bufLen);
extern uint16_t crc16(uint8_t* buf, const uint16_t bufLen, const uint16_t startCRC, const uint16_t startBit, const uint16_t len_bits);