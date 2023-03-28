Changelog v0.6.0

## General
* improved night time calculation time to 1 minute after last communication pause #515
* refactored code for better readability
* improved Hoymiles communication (retransmits, immediate power limit transmission, timing at all)
* renamed firmware binaries
* add login / logout to menu
* add display support for `SH1106`, `SSD1306`, `Nokia` and `ePaper 1.54"` (ESP32 only)
* add yield total correction - move your yield to a new inverter or correct an already used inverter
* added import / export feature
* added `Prometheus` endpoints
* improved wifi connection and stability (connect to strongest AP)
* addded Hoymiles alarm IDs to log
* improved `System` information page (eg. radio statitistics)
* improved UI (responsive design, (optional) dark mode)
* improved system stability (reduced `heap-fragmentation`, don't break settings on failure) #644, #645
* added support for 2nd generation of Hoymiles inverters, MI series
* improved JSON API for more stable WebUI
* added option to disable input display in `/live` (`max-power` has to be set to `0`)
* updated documentation
* improved settings on ESP32 devices while setting SPI pins (for `NRF24` radio)

## MqTT
* added `comm_disabled` #529
* added fixed interval option #542, #523
* improved communication, only required publishes
* improved retained flags
* added `set_power_limit` acknowledge MQTT publish #553
* added feature to reset values on midnight, communication pause or if the inverters are not available
* partially added Hoymiles alarm ID
* improved autodiscover (added total values on multi-inverter setup)
* improved `clientID` a part of the MAC address is added to have an unique name
