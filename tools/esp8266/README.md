## Table of Contents

- [Overview](#overview)
- [Compile](#compile)
  * [Optional Configuration before compilation](#optional-configuration-before-compilation)
- [Flash ESP with Firmware](#flash-esp-with-firmware)
- [Usage](#usage)
- [Compatiblity](#compatiblity)
- [Used Libraries](#used-libraries)
- [Contact](#contact)

***

## Overview

This page describes how the module of a Wemos D1 mini and ESP8266 is wired to the radio module, flashed and how the further steps are to communicate with the WR HM series.

The NRF24L01+ radio module is connected to the
standard SPI pins:
- SCLK (Signal Clock),
- MISO (Master In Slave Out) and
- MOSI (Master Out Slave In)

Additional there are 3 pins, which can be set individual:
- CS (Chip Select),
- CE (Chip Enable) and
- IRQ (Interrupt)

These pins can be changed from the /setup URL

#### Compatiblity
For now the following inverters should work out of the box:
- HM300
- HM350
- HM400
- HM600
- HM700
- HM800
- HM1000?
- HM1200
- HM1500

The NRF24L01+ radio module is connected to the standard SPI pins. 
Additional there are 3 pins, which can be set individual: CS, CE and IRQ
These pins can be changed in the http://<ip-adress>/setup URL or with a click on the Setup link.

## ESP8266 electr. associate
<img src="https://github.com/grindylow/ahoy/blob/main/doc/ESP8266_nRF24L01%2B_bb.png" width="300">

## Compile

This code can be compiled using Visual Studio Code and **PlatformIO** Addon.
  
## Used Libraries 

- `ESP8266WiFi` 1.0
- `DNSServer` 1.1.0
- `Ticker` 1.0
- `ESP8266HTTPUpdateServer` 1.0
- `Time` 1.6.1
- `RF24` 1.4.5
- `PubSubClient` 2.8
- `ArduinoJson` 6.19.4

### Optional Configuration before compilation

- number of supported inverters (set to 3 by default) `config.h`
- DTU radio id `config.h` (default = 1234567801)
- unformated list in webbrowser `/livedata` `config.h`, `LIVEDATA_VISUALIZED`

Alternativly, instead of modifying `config.h`, `config_override_example.h` can be copied to `config_override.h` and customized.
config_override.h is excluded from version control and stays local.

## Flash ESP with Firmware

#### nodemcu-pyflasher (easy way)
1. download the flash-tool [nodemcu-pyflasher](https://github.com/marcelstoer/nodemcu-pyflasher)
2. download latest release bin-file from [ahoy_](https://github.com/grindylow/ahoy/releases)
3. connect the target device with your pc.
4. Set the correct serial port and select the correct *.bin file 
5. click now on "Flash NodeMCU"

1. flash the ESP with the compiled firmware using the UART pins or any preinstalled firmware with OTA capabilities
2. repower the ESP
3. the ESP will start as access point (AP) if there is no network config stored in its eeprom
4. connect to the AP, you will be forwarded to the setup page

X. configure your WiFi settings, save, repower
Y. check your router or serial console for the IP address of the module. You can try ping the configured device name as well.
  
! ATTENTION: If you update from a very low version to the newest, please make sure to wipe all flash data!

## pages
| page | output |
| ---- | ------ |
| /uptime | 0 Days, 01:37:34; now: 2022-08-21 11:13:53 |
| /reboot | reboot dtu device |
| /erase |    |
| /factory |    |
| /setup |    |
| /save | open the setup site |
| /cmdstat | show stat from the home site | 
| /visualization |     |
| /livedata |     |
| /json | json output from the livedata |
| /api |    |

## Usage

The webinterface has the following abilities:
- OTA Update (over the air update)
- Configuration (Wifi, inverter(s), NTP Server, Pinout, MQTT, Amplifier Power Level, Debug)
- visual display of the connected inverters / modules
- some statistics about communication (debug)

The serial console will print the converted values which were read out of the inverter(s)
  
### MQTT command to set the DTU without webinterface
 [Read here](https://github.com/grindylow/ahoy/blob/development02/tools/esp8266/User_Manual.md)

 ## Todo's [See this post](https://github.com/grindylow/ahoy/issues/142)

- [ ]  Wechsel zu AsyncWebServer und ElegantOTA für Stabilität
- [x]  klarer Scheduler / Task manager, der ggf. den Receive Task priorisieren kann
- [x]  Device Info Kommandos (Firmware Version, etc.) über das Dashboard anzeigen [Device Information ( `0x15` `REQ_ARW_DAT_ALL` ) SubCmd Kommandos #145](https://github.com/grindylow/ahoy/issues/145)
- [ ]  AlarmData & AlarmUpdate Parsen und auf eigener Seite darstellen
------------------ SWIM LANE ---------------------------
- [ ]  Device Control Kommandos aus dem Setup ermöglichen (TurnOn, TurnOff, Restart, ActivePower Limit, ReactivePower Limit, SetPowerFactor, etc.)
- [ ]  Settings exportieren / importieren (API/UI)
- [ ]  Settings in settings.ini speichern (LittleFS statt EEPROM) [Settings in settings.ini speichern (LittleFS statt EEPROM) #164](https://github.com/grindylow/ahoy/issues/164)
- [ ]  Homepage aufräumen nur ein Status (aktuell drei AJAX Calls /uptime, /time, /cmdstat)
- [ ]  app.cpp aufräumen und in hmRadio / hmProtokollGen3 auslagern
- [ ]  MI Wechselrichter unterstützen (miSystem, miInverter, miDefines, miProtokollGen2 etc.)
- [ ]  nRF24 Interrupt Handling sinnvoll oder warum macht die nRF24 Bibliothek ständig `0x07` Statusabfragen [NRF24 polling trotz aktiviertem IRQ #83](https://github.com/grindylow/ahoy/issues/83)
- [ ]  Debug Level im Setup änderbar -auch Livedata Visualisierung abschalten ?
- [ ]  MQTT Discovery (HomeAssistant) im Setup optional machen
- [x]  MQTT Subscribe nur beim Reconnect [Das subscribe in der Reconnect Procedure sollte doch nur nach einem conect ausgeführt werden und nicht bei jedem Duchlauf #139](https://github.com/grindylow/ahoy/issues/139)

## Contact
We run a Discord Server that can be used to get in touch with the Developers and Users.

https://discord.gg/WzhxEY62mB
