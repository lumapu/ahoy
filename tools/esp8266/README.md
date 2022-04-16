## OVERVIEW

This code was tested on a ESP8266 - ESP-07 module. Many parts of the code are based on 'Hubi's code, which can be found here: <https://www.mikrocontroller.net/topic/525778?page=3#7033371>

The NRF24L01+ radio module is connected to the standard SPI pins. Additional there are 3 pins, which can be set individual:

- IRQ - Pin 4
- CE - Pin 5
- CS - Pin 15


## Compile

This code can be compiled using Arduino. The settings were:

- Board: Generic ESP8266 Module
- Flash-Size: 1MB (FS: none, OTA: 502kB)


## Flash ESP with firmware

1. flash the ESP with the compiled firmware using the UART pins or any preinstalled firmware with OTA capabilities
2. repower the ESP
3. the ESP will start as access point (AP) if there is no network config stored in its eeprom
4. connect to the AP, you will be forwarded to the setup page
5. configure your WiFi settings, save, repower
6. check your router for the IP address of the module


## Usage

Connect the ESP to power and to your serial console. The webinterface is currently only used for OTA and config.
The serial console will print all information which is send and received.


## Known Issues

- only command 0x81 is received


## USED LIBRARIES

- `Time`
- `RF24`

