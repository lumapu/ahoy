#ifndef __HM_CRC_H
#define __HM_CRC_H

#define BITS_TO_BYTES(x)  (((x)+7)>>3)
#define BYTES_TO_BITS(x)  ((x)<<3)

extern uint16_t crc16_modbus(uint8_t *puchMsg, uint16_t usDataLen);
extern uint8_t crc8(uint8_t *buf, const uint16_t bufLen);
extern uint16_t crc16(uint8_t* buf, const uint16_t bufLen, const uint16_t startCRC, const uint16_t startBit, const uint16_t len_bits);

//#define OUTPUT_DEBUG_INFO

#define CRC8_INIT               0x00
#define CRC8_POLY               0x01

#define CRC16_MODBUS_POLYNOM    0xA001

uint8_t crc8(uint8_t buf[], uint16_t len) {
    uint8_t crc = CRC8_INIT;
    for(uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for(uint8_t b = 0; b < 8; b ++) {
            crc = (crc << 1) ^ ((crc & 0x80) ? CRC8_POLY : 0x00);
        }
    }
    return crc;
}

uint16_t crc16_modbus(uint8_t buf[], uint16_t len) {
    uint16_t crc = 0xffff;
    uint8_t lsb;

    for(uint8_t i = 0; i < len; i++) {
        crc = crc ^ buf[i];
        for(int8_t b = 7; b >= 0; b--) {
            lsb = (crc & 0x0001);
            if(lsb == 0x01)
                crc--;
            crc = crc >> 1;
            if(lsb == 0x01)
                crc = crc ^ CRC16_MODBUS_POLYNOM;
        }
    }

    return crc;
}

// NRF24 CRC16 calculation with poly 0x1021 = (1) 0001 0000 0010 0001 = x^16+x^12+x^5+1
uint16_t crc16(uint8_t *buf, const uint16_t bufLen, const uint16_t startCRC, const uint16_t startBit, const uint16_t len_bits)
{
	uint16_t crc = startCRC;
	if ((len_bits > 0) && (len_bits <= BYTES_TO_BITS(bufLen)))
	{
		// The length of the data might not be a multiple of full bytes.
		// Therefore we proceed over the data bit-by-bit (like the NRF24 does) to
		// calculate the CRC.
		uint16_t data;
		uint8_t byte, shift;
		uint16_t bitoffs = startBit;

		// Get a new byte for the next 8 bits.
		byte = buf[bitoffs >> 3];
#ifdef OUTPUT_DEBUG_INFO
		printf_P(PSTR("\nStart CRC %04X, %u bits:"), startCRC, len_bits);
		printf_P(PSTR("\nbyte %02X:"), byte);
#endif
		while (bitoffs < len_bits + startBit)
		{
			shift = bitoffs & 7;
			// Shift the active bit to the position of bit 15
			data = ((uint16_t)byte) << (8 + shift);
#ifdef OUTPUT_DEBUG_INFO
			printf_P(PSTR(" bit %u %u,"), shift, data & 0x8000 ? 1 : 0);
#endif
			// Assure all other bits are 0
			data &= 0x8000;
			crc ^= data;
			if (crc & 0x8000)
			{
				crc = (crc << 1) ^ 0x1021; // 0x1021 = (1) 0001 0000 0010 0001 = x^16+x^12+x^5+1
			}
			else
			{
				crc = (crc << 1);
			}
			++bitoffs;
			if (0 == (bitoffs & 7))
			{
				// Get a new byte for the next 8 bits.
				byte = buf[bitoffs >> 3];
#ifdef OUTPUT_DEBUG_INFO
				printf_P(PSTR("crc %04X:"), crc);
				if (bitoffs < len_bits + startBit)
					printf_P(PSTR("\nbyte %02X:"), byte);
#endif
			}
		}
	}
	return crc;
}

#endif
