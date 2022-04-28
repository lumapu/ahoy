#ifndef __HM_PACKETS_H
#define __HM_PACKETS_H

#include "hm_crc.h"

class HM_Packets
{
private:
	uint32_t unixTimeStamp;

	void prepareBuffer(uint8_t *buf);
	void copyToBuffer(uint8_t *buf, uint32_t val);
	void copyToBufferBE(uint8_t *buf, uint32_t val);

public:
	void SetUnixTimeStamp(uint32_t ts);
	void UnixTimeStampTick();

	int32_t GetTimePacket(uint8_t *buf, uint32_t wrAdr, uint32_t dtuAdr);
	int32_t GetCmdPacket(uint8_t *buf, uint32_t wrAdr, uint32_t dtuAdr, uint8_t mid, uint8_t cmd);
};

void HM_Packets::SetUnixTimeStamp(uint32_t ts)
{
	unixTimeStamp = ts;
}

void HM_Packets::UnixTimeStampTick()
{
	unixTimeStamp++;
}

void HM_Packets::prepareBuffer(uint8_t *buf)
{
	// minimal buffer size of 32 bytes is assumed
	memset(buf, 0x00, 32);
}

void HM_Packets::copyToBuffer(uint8_t *buf, uint32_t val)
{
	buf[0]= (uint8_t)(val >> 24);
	buf[1]= (uint8_t)(val >> 16);
	buf[2]= (uint8_t)(val >> 8);
	buf[3]= (uint8_t)(val & 0xFF);
}

void HM_Packets::copyToBufferBE(uint8_t *buf, uint32_t val)
{
	memcpy(buf, &val, sizeof(uint32_t));
}

static uint8_t cid = 0;

int32_t HM_Packets::GetTimePacket(uint8_t *buf, uint32_t wrAdr, uint32_t dtuAdr)
{
	prepareBuffer(buf);

	buf[0] = 0x15;
	copyToBufferBE(&buf[1], wrAdr);
	copyToBufferBE(&buf[5], dtuAdr);
	buf[9] = 0x80;
  buf[10] = 0x0B;        //0x0B;  0x03 0x11
	buf[11] = 0x00;

	copyToBuffer(&buf[12], unixTimeStamp);

	buf[19] = 0x05;

	// CRC16
	uint16_t crc16 = crc16_modbus(&buf[10], 14);
	buf[24] = crc16 >> 8;
	buf[25] = crc16 & 0xFF;

	// crc8
	buf[26] = crc8(&buf[0], 26);

	return 27;
}

int32_t HM_Packets::GetCmdPacket(uint8_t *buf, uint32_t wrAdr, uint32_t dtuAdr, uint8_t mid, uint8_t cmd)
{
	buf[0] = mid;
	copyToBufferBE(&buf[1], wrAdr);
	copyToBufferBE(&buf[5], dtuAdr);
	buf[9] = cmd;

	// crc8
	buf[10] = crc8(&buf[0], 10);

	return 11;
}

#endif
