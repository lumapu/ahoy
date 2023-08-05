Changelog v0.7.25

* ✨ added 'HMS' and 'HMT' support with 'CMT2300A' radio module and `ESP32`
* improved MqTT
* added more display types
* changed maximum number of inverters: ESP32: `16`, ESP8266: `4`
* added option to connect to hidden SSID WiFi
* AP password is configurable now
* add option to communicate with inverters even if no time sync is possible
* add option to reboot Ahoy perodically at midnight
* add time, date and Ahoy-Version number to JSON export
* changed date-time format to ISO format in web UI
* added inverter state machine to archive better its state
* added for NTP sync more info to web UI about the sync itself
* increased to latest library versions (MqTT, RF24 and JSON)
* minor UI improvements
* several bug fixes
