# Development Changes

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
