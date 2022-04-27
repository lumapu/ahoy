

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
