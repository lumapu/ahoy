## OVERVIEW

This code is intended to run on a Wemos D1mini or similar. The code is based on 'Hubi's code, which can be found here: <https://www.mikrocontroller.net/topic/525778?page=3#7033371>

The NRF24L01+ radio module is connected to the standard SPI pins. Additional there are 3 pins, which can be set individual: CS, CE and IRQ
These pins can be changed from the /setup URL


## Compile

This code can be compiled using Visual Studio Code and **PlatformIO** Addon. The settings were:

- Board: Generic ESP8266 Module
- Flash-Size: 1MB (FS: none, OTA: 502kB)
- Install libraries (not included in the Arduino IDE 1.8.19):
  - Time            Arduino Time library (TimeLib.h)
  - RF24            Optimized high speed nRF24L01+ driver class documentation
  - PubSubClient    A client library for MQTT messaging. By Nick O'Leary
  - ArduinoJson     Arduino Json library

### Optional Configuration before compilation

- number of supported inverters (set to 3 by default) `defines.h`
- DTU radio id `hmRadio.h`
- unformated list in webbrowser `/livedata` `config.h`, `LIVEDATA_VISUALIZED`


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

- HM350
- HM400
- HM600
- HM700
- HM800
- HM1200
- HM1500

## USED LIBRARIES

- `ESP8266WiFi` 1.0
- `DNSServer` 1.1.0
- `Ticker` 1.0
- `ESP8266HTTPUpdateServer` 1.0
- `Time` 1.6.1
- `RF24` 1.4.2
- `PubSubClient` 2.8
- `ArduinoJson` 6.19.4

## Changelog

(*) EEPROM changes require settings to be changed, your settings will be overwritten and need to be set again!

- v0.4.25 added default SERIAL/MQTT/SEND_INTERVAL #100, fixed env:node_mcu_v2 build #101
- v0.4.24 added fixes for #63, #88, #93. revert #36 (*) EEPROM changes
- v0.4.23 added workflow, fix index.html to load inverter info immediately, changed timestamp to 1 for stand alone ESP #90, Implement MQTT discovery for Home Assistant
- v0.4.22 compiles with PlatformIO
- v0.4.21 reduced warnings
- v0.4.20 improved setup (if no data is in EEprom), improved NRF24 Pinout regarding to #36, Standard Pinout should be now: #36 (comment), add JSON output, fix favicon, improve eeprom default settings (*) EEPROM changes
- v0.4.19 updated debug messages: now 5 different levels are available, fixed CRC loop issue, add fritzing/schematics for Arduino, Raspberry Pi and NodeMCU
- v0.4.18 Creative Commons NC-SA-BY v3.0  license included, tried to increase stability, fix NRF24 CRClength, add debug & documentation links,  added variable error messages using #pragma error
- v0.4.17 add printed circuit board layout, more debug output (#retransmits), improved loop counters (*) EEPROM changes
- v0.4.16 request only one inverter per loop (#53 (comment)), mqtt loop interval calculated by # of inverters and inverter request interval, limit maximum number of retries, added feature request #62 (readable names for channels), improved setup page, added javascript to hide / show channel fields (*) EEPROM changes
- v0.4.15 reduced debug messages, fixes after merge
- v0.4.14 added RX channel 40, improved RF24 ISR, reduced AP active time to 60s (will be increase once a client is connected), added `yield` without success -> random reboot (cause 4) (*) EEPROM changes
- v0.4.13 rename to AHOY-DTU, add RX channel 40, update stats on index based on mSendInterval, MQTT Interval, EEPROM CRC settings, fix #56 v0.4.10 ESP8266 stuck in boot loop
- v0.4.12 version skipped ?
- v0.4.11 inverter dependent mqtt (is avail), implemented heap stats #58, inserted 'break' in ISR while loop
- v0.4.10 reduced heap size (>50%) by using 'F()' for (nearly) all static strings, added Wemos D1 case STL files
- v0.4.9 try to fix mqtt and wifi loss issue #52, document libraries (*) EEPROM changes
- v0.4.8 moved mqtt loop out of checkTicker as mentioned in #49, added irritation and efficiency calculations, improved style (*) EEPROM changes
- v0.4.7 version skipped ?
- v0.4.6 version skipped ?
- v0.4.5 fix #38 4-channel inverter current assignment, added last received timestamp in /hoymiles livedata web page #47, improved style.css, improved NTP as described in #46
- v0.4.4 added free heap, mentioned in #24 (added in serial print, status on index and mqtt), fixed #45, AC current by factor 10 too high, fixed failed payload counter
- v0.4.3 fixed #41 HM800 Yield total and Yield day were mixed around. Found issue while comparing to Python version, fixed #43 HM350 channel 2 is displayed in Live-View, added #42 YieldTotal and YieldTotal Day for HM600 - HM800 inverters
- v0.4.2 fix #39 Assignment 2-Channel inverters (HM-600, HM-700, HM-800)
- v0.4.1 multi inverter support, full re transmit included
- v0.4.0 complete payload processed (and crc checked), inverter type is defined by serial number, serial debug can be switched live (using setup), Note: only one inverter is supported for now!
- v0.3.9 fix #26 ticker / interval in app.cpp
- v0.3.8 improved stability (in comparison to 0.3.7), reset wifi AP timout once a client is detected, fix #26 wrong variable reset
- v0.3.7 added rx channel switching, switched to crc8 check for valid packet-payload
- v0.3.6 improved tickers, only one ticker is active, added feature to use the ESP as access point for all the time, added serial features to setup
- v0.3.5 fixed erase settings, fixed behavior if no MQTT IP is set (the system was nearly unusable because of delayed responses), fixed Station / AP WiFi on startup -> more information will be printed to the serial console, added new ticker for serial value dump
- v0.3.4 added config.h for general configuration, added option to compile WiFi SSID + PWD to firmware, added option to configure WiFi access point name and password, added feature to retry connect to station WiFi (configurable timeouts), updated index.html, added option for factory reset, added info about project on index.html, moved "update" and "home" to footer, fixed #23 HM1200 yield day unit, fixed DNS name of ESP after setup (some commits before)
- v0.3.3 converted to "poor-man-ticker" using millis() for uptime, send and mqtt, added inverter overview, added send count to statistics
- v0.3.2 compile of merge, binary published on https://www.mikrocontroller.net/topic/525778?goto=7051413#7051413
- v0.3.1 fix: doCalculations was not called
- v0.3.0 version 0.3.0, added unit test
