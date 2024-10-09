Changelog v0.8.140

* added HMS-400-1T support (serial number 1125...)
* added further ESP8266 versions (-all, -minimal) because of small ressources on ESP8266
* added some Gridprofiles
* added support for characters in serial number of inverter (A-F)
* added default coordinates on fresh install, needed for history graph on display and WebUI
* added option to reset values on communication start (sunrise)
* added max inverter temperature to WebUI
* added yield day to history graph
* added script and [instructions](../manual/factory_firmware.md) how to generate factory firmware which includes predefined settings
* added button for downloading coredump (ESP32 variants only) to `/system`. Once a crash happens the reason can be checked afterwards (even after a reboot)
* added support of HERF inverters, serial number is converted in Javascript
* added device name to HTML title
* added feature to restart Ahoy using MqTT
* added feature to publish MqTT messages as JSON as well (new setting)
* add timestamp to JSON output
* improved communication to inverter
* improved translation to German
* improved HTML pages, reduced in size by only including relevant contents depending by chip type
* improved history graph in WebUI
* improved network routines
* improved Wizard
* improved WebUI by disabling upload and import buttons when no file is selected
* improved queue, only add new object once they not exist in queue
* improved MqTT `OnMessage` (threadsafe)
* improved read of alarms, prevent duplicates, update alarm time if there is an update
* improved alarms are now sorted in ascending direction
* improved by prevent add inverter multiple times
* improved sending active power controll commands
* improved refresh routine of ePaper, full refresh each 12h
* redesigned WebUI on `/system`
* changed MqTT retained flags
* change MqTT return value of power limit acknowledge from `boolean` to `float`. The value returned is the same as it was set to confirm reception (not the read back value)
* converted ePaper and Ethernet to hal-SPI
* combined Ethernet and WiFi variants - Ethernet is now always included, but needs to be enabled if needed
* changed: Ethernet variants (W5500) now support WiFi as fall back / configuration
* switch AsyncWebserver library
* fixed autodiscovery for homeassistant
* fix reset values functionality
* fix read back of active power control value, now it has one decimal place
* fix NTP issues
* fixed MqTT discovery field `ALARM_MES_ID`
* fix close button color of modal windows in dark mode
* fixed calculation of max AC power
* fixed reset values at midnight if WiFi isn't available
* fixed HMT-1800-4T number of inputs
* fix crash if invalid serial number was set -> inverter will be disabled automatically
* fixed ESP8266, ESP32 static IP
* fixed ethernet MAC address read back
* update several libraries to more recent versions
* removed `yield efficiency` because the inverter already calculates correct

full version log: [Development Log](https://github.com/lumapu/ahoy/blob/development03/src/CHANGES.md)
