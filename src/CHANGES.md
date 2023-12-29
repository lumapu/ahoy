Changelog v0.8.34

* improve communication, all inverters are polled during interval
* add hardware info, click on inverter name (in `/live`)
* added RSSI info for HMS and HMT inverters (MqTT + REST API)
* add signal strength for NRF24
* change ePaper text to symbols
* 2.42" display (SSD1309) integration
* moved active power control to modal in `/live` view (per inverter) by click on current APC state
* moved radio statistics into the inverter - each inverter has now seperate statistics which can be accessed by click on the footer in `/live`
* serveral improvements for MI inverters
* add total AC Max Power to WebUI
* added inverter-wise power-level, frequency and night-communication
* added privacy mode option (`/serial`)
* added button `copy to clipboard` to `/serial`
* added ms to `/serial`
* fixed range of HMS / HMT frequencies to 863 to 870 MHz
* read grid profile (`live` -> click inverter name -> `show grid profile`)
* moved MqTT info to `/system`
* beautified `/system`
* added current AC-Power to `/index` page and removed version
* added default pins for opendtu-fusion-v1 board
* added default pins for ESP32 with CMT2300A (matching OpenDTU)
* added ethernet build for fusion board
* added ESP32-S2 to github actions
* added ESP32-S3-mini to github actions
* added ESP32-C3-mini to github actions
* add option to reset 'max' values on midnight
* luminance of display can be changed during runtime
* added developer option to use 'syslog-server' instead of 'web-serail'
* added / renamed alarm codes
* changed MqTT alarm topic, removed retained flag
* added default input power to `400` while adding new inverters

full version log: [Development Log](https://github.com/lumapu/ahoy/blob/development03/src/CHANGES.md)