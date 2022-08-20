# Changelog

- v0.5.15
    * Bug fix: mqtt payload handling (thx @klahus1, silverserver)
    * Bug fix: eeprom alignment fixed (thx @klahus1)
    * mqtt reconnect improvements (thx @tastendruecker123 , @HorstG-57 )
    * simple command scheduler (one place fifo)
    * InverterDevInform_All Command parser and output to mqtt
    * New workflow to build github release
    * Introduction of a command queue (like OpenDTU)
    * Feedback from inverter for actual power limit via InfoCmd -> SystemConfigPara (0x05) placed in visualization
    * REST API will enqueue a new info command (all commands supported)
    * Change in power limit will (Setup, MQTT or REST API) enqueues a new infocmd request to get actual power limit
    * Actual power limit is available under MQTT topic <TOPIC>/<INVERTER-NAME>/ch0/PowerLimit ALWAYS in percent
    * Firmware information will be requested automatically up on start of dtu
    * Added User_Manual.md

- v0.5.14
- v0.5.13
- v0.5.12
- v0.5.11
- v0.5.10
- v0.5.9 *fix PowerLimit PowerPFDev.Desc=0x0001 for permanent
- v0.5.8 *fix #146 device name in setup
- v0.5.7 *add collapsible setup
- v0.5.6 *fix only MQTT sub after the first loop in a conenction
- v0.5.5 *fixed MQTT sub only after connection is established (HorstG-57)
         + added in app.cpp some compiler if statements
         *fix: compile possible for non repository versions (if project was download as zip - lumapu)
         *fix README.md - Update line 69 (`RF24` 1.4.2 -> 1.4.5) (DanielR92)
         *Update hmRadio.h (lumapu)
- v0.5.4 + added Github report text with a URL (aschiffler)
         + added auto_firmware_version.py for GIT_HASH
         + added switch case AlarmData/AlarmUpdate
- v0.5.3 #Bugfix #125 PowerLimit
         + prototype webapi to get info, improved pwr limit  (aschiffler)
         + Merge remote-tracking branch 'upstream/main' into pwrlimit
- v0.5.2 add #114 ntp_server_name and port to eeprom
         + stefan123t added some functions (devcontrol/cbMqtt/...)
- v0.5.1 *Merge branch 'upstream/HEAD' into control
         *update revision  (0.4.26 -> 0.5.1)
- v0.4.26 first poc for power set via mqtt
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
(*) EEPROM changes require settings to be changed, your settings will be overwritten and need to be set again!
