## Table of Contents

- [Overview](#overview)
- [Compatiblity](#compatiblity)
- [Things needed](#things-needed)
- [Wiring things up](#wiring-things-up)
    + [ESP8266 wiring example](#esp8266-wiring-example)
- [Flash the Firmware on your Ahoy DTU Hardware](#flash-the-firmware-on-your-ahoy-dtu-hardware)
    + [Compiling your own Version (the geek way)](#compiling-your-own-version)
      - [Optional Configuration before compilation](#optional-configuration-before-compilation)
    + [Using a ready-to-flash binary using nodemcu-pyflasher (the easy way)](#using-a-ready-to-flash-binary-using-nodemcu-pyflasher)
- [Connect to your Ahoy DTU](#connect-to-your-ahoy-dtu)
    + [Your Ahoy DTO is very verbose using the Serial Console](#your-ahoy-dto-is-very-verbose-using-the-serial-console)
    + [Connect to the Ahoy DTU Webinterface using your Browser](#connect-to-the-ahoy-dtu-webinterface-using-your-browser)
      - [HTTP based Pages](#http-based-pages)
- [MQTT command to set the DTU without webinterface](#mqtt-command-to-set-the-dtu-without-webinterface)
- [Used Libraries](#used-libraries)
- [Contact](#contact)
- [ToDo's - remove when done](#todo)

***

## Overview

This page describes how the module of a Wemos D1 mini and ESP8266 is wired to the radio module and is flashed with the latest Firmware.<br/>
Further information will help you to communicate to the compatible converters.

## Compatiblity
For now the following Inverters should work out of the box:

Hoymiles Inverters
- HM300
- HM350
- HM400
- HM600
- HM700
- HM800
- HM1000?
- HM1200
- HM1500

TSun Inverters:
- TSOL-350
- TSOL-400
- othery may work as well (need to be veryfied).


## Things needed
In order to build your own Ahoy DTU, you will need some things.<br/>
This list is not closing as the Maker Community offers more Boards than we could cover in this Readme.<br/><br/>

We suggest to use a WEMOS D1 mini Board as well as a NRF24L01+ Breakout Board.<br/>
Make sure it has the "+" in its name as we depend on some features provided with the plus-variant.<br/>
Any other ESP8266 Board with at least 4MBytes of ROM could work as well, depending on your skills.


## Wiring things up

The NRF24L01+ radio module is connected to the
standard SPI pins:
- SCLK (Signal Clock),
- MISO (Master In Slave Out) and
- MOSI (Master Out Slave In)

*These pins need to be configured in the config.h.*

Additional, there are 3 pins, which can be set individual:
- CS (Chip Select),
- CE (Chip Enable) and
- IRQ (Interrupt)

*These pins can be changed from the /setup URL.*

#### ESP8266 wiring example
ToDo: (this one needs to be reworked - also a generified one would be helpful)
<img src="https://github.com/grindylow/ahoy/blob/main/doc/ESP8266_nRF24L01%2B_bb.png" width="300">




## Flash the Firmware on your Ahoy DTU Hardware
Once your Hardware is ready to run, you need to flash the Ahoy DTU Firmware to your Board.
You can either build your own using your own configuration or use one or our pre-compiled generic builds.


#### Compiling your own Version
This information suits you if you want to configure and build your own firmware.

This code comes to you as a **PlatformIO** project and can be compiled using the **PlatformIO** Addon.<br/>
Visual Studio Code, AtomIDE and other IDE's support the PlatformIO Addon.<br/>
If you do not want to compile your own build, you can use one of our ready-to-flash binaries.

##### Optional Configuration before compilation

- number of supported inverters (set to 3 by default) `config.h`
- DTU radio id `config.h` (default = 1234567801)
- unformated list in webbrowser `/livedata` `config.h`, `LIVEDATA_VISUALIZED`

Alternativly, instead of modifying `config.h`, `config_override_example.h` can be copied to `config_override.h` and customized.
config_override.h is excluded from version control and stays local.


#### Using a ready-to-flash binary using nodemcu-pyflasher
This information suits you if you just want to use an easy way.

1. download the flash-tool [nodemcu-pyflasher](https://github.com/marcelstoer/nodemcu-pyflasher)
2. download latest release bin-file from [ahoy_](https://github.com/grindylow/ahoy/releases)
3. open flash-tool and connect the target device with your pc.
4. Set the correct serial port and select the correct *.bin file 
5. click on "Flash NodeMCU"
6. flash the ESP with the compiled firmware using the UART pins or
7. repower the ESP
8. the ESP will start as access point (AP) if there is no network config stored in its eeprom
9. connect to the AP, you will be forwarded to the setup page
10. configure your WiFi settings, save, repower
11. check your router or serial console for the IP address of the module. You can try ping the configured device name as well.
  
  
Once your Ahoy DTU is running, you can use the Over The Air (OTA) capabilities to update ypur firmware.

  
! ATTENTION: If you update from a very low version to the newest, please make sure to wipe all flash data!


  



## Connect to your Ahoy DTU
When everything is wired up and the firmware is flashed, it is time to connect to your Ahoy DTU.


#### Your Ahoy DTO is very verbose using the Serial Console
 When connected to your computer, you can open a Serial Console to obtain additional information.
 This might be useful in case of any troubles that might occur as well as to simply obtain information about the converted values which were read out of the inverter(s).
 
 
#### Connect to the Ahoy DTU Webinterface using your Browser
 After you have sucessfully flashed and powered your Ahoy DTU, you can access it via your Browser.
 If your Ahoy DTU was able to log into the configured WiFi Network, it will try to obtain an IP-Adress from your local DHCP Server (in most cases thats your Router).
 In case it could not connect to your configured Network, it will provide its own WiFi Network that you can connect to for furter configuration. The WiFi SSID ("Name") and Passwort is configured in the config.h and defaults to the SSID "AHOY-DTU" with the Passwort "esp_8266".
 The Ahox DTU will keep that Network open for a certain amount of time (also configurable in the config.h and defaults to 60secs). If nothing connects to it and that time runs up, it will retry to connect to the configured network an so on.
 
 If yonnected to your local Network, you just have to find out the used IP Address. In must cases your Router will give you a hint.
 If you connect to the WiFi the Ahoy DTU opens in case it could not connect to any other Network, the IP-Address of your Ahoy DTU is 192.168.0.1.
 Just open the IP-Address in your browser.
 
 The webinterface has the following abilities:
- OTA Update (Over The Air Update)
- Configuration (Wifi, inverter(s), NTP Server, Pinout, MQTT, Amplifier Power Level, Debug)
- visual display of the connected inverters / modules
- some statistics about communication (debug)
 
 
##### HTTP based Pages
 To take control of your Ahoy DTU, you can directly call one of the following sub-pages (e.g. http://192.168.0.1/setup ).
 
| page | use | output |
| ---- | ------ | ------ |
| /uptime | displays the uptime uf your Ahoy DTU | 0 Days, 01:37:34; now: 2022-08-21 11:13:53 |
| /reboot | reboots the Ahoy DTU | |
| /erase | erases the EEPROM |    |
| /factory | resets to the factory defaults configured in config.h |    |
| /setup | opens the setup page |    |
| /save | | |
| /cmdstat | show stat from the home page | |
| /visualization | displays the information from your converter |     |
| /livedata | displays the live data |     |
| /json | gets live-data in JSON format | json output from the livedata |
| /api | |    |

 

## MQTT command to set the DTU without webinterface
 [Read here](https://github.com/grindylow/ahoy/blob/development02/tools/esp8266/User_Manual.md)



## Used Libraries 

- `ESP8266WiFi` 1.0
- `DNSServer` 1.1.0
- `Ticker` 1.0
- `ESP8266HTTPUpdateServer` 1.0
- `Time` 1.6.1
- `RF24` 1.4.5
- `PubSubClient` 2.8
- `ArduinoJson` 6.19.4


## Contact
We run a Discord Server that can be used to get in touch with the Developers and Users.

https://discord.gg/WzhxEY62mB



## ToDo

[See this post](https://github.com/grindylow/ahoy/issues/142)

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
