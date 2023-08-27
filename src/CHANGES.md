# Development Changes

## 0.7.42 - 2023-08-27
* fix ePaper for opendtufusion_v2.x boards (Software SPI)
* add signal strength for NRF24 - PR #1119
* refactor wifi class to support ESP32 S2 PR #1127
* update platform for ESP32 to 6.3.2
* fix opendtufusion LED (were mixed)
* fix `last_success` transmitted to often #1124

## 0.7.41 - 2023-08-26
* merge PR #1117 code spelling fixes #1112
* alarms were not read after the first day

## 0.7.40 - 2023-08-21
* added default pins for opendtu-fusion-v1 board
* fixed hw version display in `live`
* removed development builds, renamed environments in `platform.ini`

## 0.7.39 - 2023-08-21
* fix background color of invalid inputs
* add hardware info (click in `live` on inverter name)

## 0.7.38 - 2023-08-21
* reset alarms at midnight (if inverter is not available) #1105, #1096
* add option to reset 'max' values on midnight #1102
* added default pins for CMT2300A (matching OpenDTU)

## 0.7.37 - 2023-08-18
* fix alarm time on WebGui #1099
* added RSSI info for HMS and HMT inverters (MqTT + REST API)

## 0.7.36
* last Release
