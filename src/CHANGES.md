Changelog v0.8.83

* added German translations for all variants
* added reading grid profile
* added decimal place for active power control (APC aka power limit)
* added information about working IRQ for NRF24 and CMT2300A to `/system`
* added loss rate to `/visualization` in the statistics window and MqTT
* added optional output to display whether it's night time or not. Can be reused as output to control battery system or mapped to a LED
* added timestamp for `max ac power` as tooltip
* added wizard for initial WiFi connection
* added history graph (still under development)
* added simulator (must be activated before compile, standard: off)
* added minimal version (without: MqTT, Display, History), WebUI is not changed! (not compiled automatically)
* added info about installed binary to `/update`
* added protection to prevent update to wrong firmware (environment check)
* added optional custom link to the menu
* added support for other regions (USA, Indonesia)
* added warning for WiFi channel 12-14 (ESP8266 only)
* added `max_power` to MqTT total values
* added API-Token authentification for external scripts
* improved MqTT by marking sent data and improved `last_success` resends
* improved communication for HM and MI inverters
* improved reading live data from inverter
* improved sending active power control command faster
* improved `/settings`: pinout has an own subgroup
* improved export by saving settings before they are exported (to have everything in JSON)
* improved code quality (cppcheck)
* seperated sunrise and sunset offset to two fields
* fix MqTT night communication
* fix missing favicon to html header
* fix build on Windows of `opendtufusion` environments (git: trailing whitespaces)
* fix generation of DTU-ID
* fix: protect commands from popup in `/live` if password is set
* fix: prevent sending commands to inverter which isn't active
* combined firmware and hardware version to JSON topics (MqTT)
* updated Prometheus with latest changes
* upgraded most libraries to newer versions
* beautified typography, added spaces between value and unit for `/visualization`
* removed add to total (MqTT) inverter setting

full version log: [Development Log](https://github.com/lumapu/ahoy/blob/development03/src/CHANGES.md)
