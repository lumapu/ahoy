# Development Changes

## 0.7.13 - 2023-07-19
* merged display PR #1027
* add date, time and version to export json #1024

## 0.7.12 - 2023-07-09
* added inverter status - state-machine #1016

## 0.7.11 - 2023-07-09
* fix MqTT endless loop #1013

## 0.7.10 - 2023-07-08
* fix MqTT endless loop #1013

## 0.7.9 - 2023-07-08
* added 'improve' functions to set wifi password directly with ESP web tools #1014
* fixed MqTT publish while appling power limit #1013
* slightly improved HMT live view (Voltage & Current)

## 0.7.8 - 2023-07-05
* fix `YieldDay`, `YieldTotal` and `P_AC` in `TotalValues` #929
* fix some serial debug prints
* merge PR #1005 which fixes issue #889
* merge homeassistant PR #963
* merge PR #890 which gives option for scheduled reboot at midnight (default off)

## 0.7.7 - 2023-07-03
* attempt to fix MqTT `YieldDay` in `TotalValues` #927
* attempt to fix MqTT `YieldDay` and `YieldTotal` even if inverters are not completly available #929
* fix wrong message 'NRF not connected' if it is disabled #1007

## 0.7.6 - 2023-06-17
* fix display of hidden SSID checkbox
* changed yield correction data type to `double`, now decimal places are supported
* corrected name of 0.91" display in settings
* attempt to fix MqTT zero values only if setting is there #980, #957
* made AP password configurable #951
* added option to start without time-sync, eg. for AP-only-mode #951

## 0.7.5 - 2023-06-16
* fix yield day reset on midnight #957
* improved tickers in `app.cpp`

## 0.7.4 - 2023-06-15
* fix MqTT `P_AC` send if inverters are available #987
* fix assignments for HMS 1CH and 2CH devices
* fixed uptime overflow #990

## 0.7.3 - 2023-06-09
* fix hidden SSID scan #983
* improved NRF24 missing message on home screen #981
* fix MqTT publishing only updated values #982

## 0.7.2 - 2023-06-08
* fix HMS-800 and HMS-1000 assignments #981
* make nrf enabled all the time for ESP8266
* fix menu item `active` highlight for 'API' and 'Doku'
* fix MqTT totals issue #927, #980
* reduce maximum number of inverters to 4 for ESP8266, increase to 16 for ESP32

## 0.7.1 - 2023-06-05
* enabled power limit control for HMS / HMT devices
* changed NRF24 lib version back to 1.4.5 because of compile problems for EPS8266

## 0.7.0 - 2023-06-04
* HMS / HMT support for ESP32 devices

## 0.6.15 - 2023-05-25
* improved Prometheus Endpoint PR #958
* fix turn off ePaper only if setting was set #956
* improved reset values and update MqTT #957

## 0.6.14 - 2023-05-21
* merge PR #902 Mono-Display

## 0.6.13 - 2023-05-16
* merge PR #934 (fix JSON API) and #944 (update manual)

## 0.6.12 - 2023-04-28
* improved MqTT
* fix menu active item

## 0.6.11 - 2023-04-27
* added MqTT class for publishing all values in Arduino `loop`

## 0.6.10 - HMS
* Version available in `HMS` branch

## 0.6.9
* last Relaese
