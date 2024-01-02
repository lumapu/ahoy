Changelog VArt (based on v0.7.36)
* Changed reconnect on WiFi lost
* Added Histroy Menu and Charts
* Added chart to Display128x64


Changelog v0.7.36

* added Ethernet variant
* fix configuration of ePaper
* fix MI inverter support
* endpoints `/api/record/live`, `/api/record/alarm`, `/api/record/config`, `/api/record/info` are obsolete
* added `/api/inverter/alarm/[ID]` to read inverter alarms
* added Alarms in Live View as modal window
* added MqTT transmission of last 10 alarms
* updated documentation
* changed `ESP8266` default NRF24 pin assignments (`D3` = `CE` and `D4` = `IRQ`)
* changed live view to gray once inverter isn't available -> fast identify if inverters are online
* added information about maximum power (AC and DC)
* updated documentation
* several small fixes
