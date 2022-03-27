Try to discover inverters with known serial numbers.

This tool will continuously scan all channels.

Requires NRF24 library from https://github.com/nRF24/RF24/ and an NRF24
module connected to the Raspberry Pi's GPIO header as described in
https://nrf24.github.io/RF24/index.html.

UPDATE: Note that this tool relies on the NRF24's AutoAck functionality.
It has since been conclusively proven that the inverters do not enable
AutoAck. Therefore this tool works quite well for discovering NRF24s,
just not the ones build into the inverters :-(
