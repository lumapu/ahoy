## OVERVIEW

This code is intended to run on a Wemos D1mini or similar. The code is based on 'Hubi's code, which can be found here: <https://www.mikrocontroller.net/topic/525778?page=3#7033371>

The NRF24L01+ radio module is connected to the standard SPI pins. Additional there are 3 pins, which can be set individual: CS, CE and IRQ
These pins can be changed from the /setup URL


## Compile

This code can be compiled using Arduino. The settings were:

- Board: Generic ESP8266 Module
- Flash-Size: 1MB (FS: none, OTA: 502kB)

### Optional Configuration before compilation

- number of supported inverters (set to 3 by default) `defines.h`
- enable channel hopping `hmRadio.h`
- DTU radio id `hmRadio.h`
- unformated list in webbrowser `/livedata` `defines.h`, `LIVEDATA_VISUALIZED`


## Flash ESP with firmware

1. flash the ESP with the compiled firmware using the UART pins or any preinstalled firmware with OTA capabilities
2. repower the ESP
3. the ESP will start as access point (AP) if there is no network config stored in its eeprom
4. connect to the AP, you will be forwarded to the setup page
5. configure your WiFi settings, save, repower
6. check your router or serial console for the IP address of the module. You can try ping the configured device name as well.


## Usage

Connect the ESP to power and to your serial console (optional). The webinterface has the following abilities:

- OTA Update (over the air update)
- Configuration (Wifi, inverter(s), Pinout, MQTT)
- visual display of the connected inverters / modules
- some statistics about communication (debug)

The serial console will print the converted values which were read out of the inverter(s)


## Compatiblity

For now the following inverters should work out of the box:

- HM400
- HM600
- HM800
- HM1200

## USED LIBRARIES

- `Time`
- `RF24`
- `PubSubClient`

