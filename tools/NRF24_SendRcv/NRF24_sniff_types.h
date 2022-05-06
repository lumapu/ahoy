/*
  This file is part of NRF24_Sniff.

  Created by Ivo Pullens, Emmission, 2014 -- www.emmission.nl
    
  NRF24_Sniff is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  NRF24_Sniff is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with NRF24_Sniff.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NRF24_sniff_types_h
#define NRF24_sniff_types_h

typedef struct _NRF24_packet_t
{
  uint32_t timestamp;
  uint8_t  packetsLost;
  uint8_t  packet[MAX_RF_PAYLOAD_SIZE];
} NRF24_packet_t;

typedef struct _Serial_header_t
{
  unsigned long timestamp;
  uint8_t  packetsLost;
  uint8_t  address[RF_MAX_ADDR_WIDTH];    // MSB first, always RF_MAX_ADDR_WIDTH bytes.
} Serial_header_t;

typedef struct _Serial_config_t
{
  uint8_t channel;
  uint8_t rate;                        // rf24_datarate_e: 0 = 1Mb/s, 1 = 2Mb/s, 2 = 250Kb/s
  uint8_t addressLen;                  // Number of bytes used in address, range [2..5]
  uint8_t addressPromiscLen;           // Number of bytes used in promiscuous address, range [2..5]. E.g. addressLen=5, addressPromiscLen=4 => 1 byte unique identifier.
  uint64_t address;                    // Base address, LSB first.
  uint8_t crcLength;                   // Length of active CRC, range [0..2]
  uint8_t maxPayloadSize;              // Maximum size of payload for nRF (including nRF header), range[4?..32]
} Serial_config_t;

#define MSG_TYPE_PACKET  (0)
#define MSG_TYPE_CONFIG  (1)

#define SET_MSG_TYPE(var,type)   (((var) & 0x3F) | ((type) << 6))
#define GET_MSG_TYPE(var)        ((var) >> 6)
#define GET_MSG_LEN(var)         ((var) & 0x3F)

#endif // NRF24_sniff_types_h
