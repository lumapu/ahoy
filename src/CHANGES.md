# Changelog

## 0.5.59
* fix night communication enable
* improved different WiFi connection scenarios (STA WiFi not found, reconnect #509, redirect for AP to configuration)
* increased MQTT user, pwd and topic length to 64 characters + `\0`. (The string end `\0` reduces the available size by one) #516

## 0.5.58
* improved stability
* improved WiFi initial connection - especially if station WiFi is not available
* removed new operators from web.h (reduce dynamic allocation)
* improved sun calculation #515, #505
* fixed WiFi auto reconnect #509
* added disable night communication flag to MQTT #505
* changed MQTT publish of `available` and `available_text` to sunset #468

## 0.5.57
* improved stability
* added icons to index.html, added WiFi-strength symbol on each page
* moved packet stats and sun to system.html
* refactored communication offset (adjustable in minutes now)

## 0.5.56
* factory reset formats entire little fs
* renamed sunrise / sunset on index.html to start / stop communication
* show system information only if called directly from menu
* beautified system.html

## 0.5.55
* fixed static IP save

## 0.5.54
* changed sunrise / sunset calculation, angle is now `-3.5` instead of original `-0.83`
* improved scheduler (removed -1 from `reload`) #483
* improved reboot flag in `app.h`
* fixed #493 no MQTT payload once display is defined

## 0.5.53
* Mono-Display: show values in offline mode #498
* improved WiFi class #483
* added communication enable / disable (to test mutliple DTUs with the same inverter)
* fix factory reset #495

## 0.5.52
* improved ahoyWifi class
* added interface class for app
* refactored web and webApi -> RestApi
* fix calcSunrise was not called every day
* added MQTT RX counter to index.html
* all values are displayed on /live even if they are 0
* added MQTT <TOPIC>/status to show status over all inverters

## 0.5.51
* improved scheduler, @beegee3 #483
* refactored get NTP time, @beegee3 #483
* generate `bin.gz` only for 1M device ESP8285
* fix calcSunrise was not called every day
* incresed number of allowed characters for MQTT user, broker and password, @DanielR92
* added NRF24 info to Systeminfo, @DanielR92
* added timezone for monochrome displays, @gh-fx2
* added support for second inverter for monochrome displays, @gh-fx2

## 0.5.50
* fixed scheduler, uptime and timestamp counted too fast
* added / renamed automatically build outputs
* fixed MQTT ESP uptime on reconnect (not zero any more)
* changed uptime on index.html to count each second, synced with ESP each 10 seconds

## 0.5.49
* fixed AP mode on brand new ESP modules
* fixed `last_success` MQTT message
* fixed MQTT inverter available status at sunset
* reordered enqueue commands after boot up to prevent same payload length for successive commands
* added automatic build for Nokia5110 and SSD1306 displays (ESP8266)

## 0.5.48
* added MQTT message send at sunset
* added monochrome display support
* added `once` and `onceAt` to scheduler to make code cleaner
* improved sunrise / sunset calculation

## 0.5.47
* refactored ahoyWifi class: AP is opened on every boot, once station connection is successful the AP will be closed
* improved NTP sync after boot, faster sync
* fix NRF24 details only on valid SPI connection

## 0.5.46
* fix sunrise / sunset calculation
* improved setup.html: `reboot on save` is checked as default

## 0.5.45
* changed MQTT last will topic from `status` to `mqtt`
* fix sunrise / sunset calculation
* fix time of serial web console

## 0.5.44
* marked some MQTT messages as retained
* moved global functions to global location (no duplicates)
* changed index.html inverval to static 10 seconds
* fix static IP
* fix NTP with static IP
* print MQTT info only if MQTT was configured

## 0.5.43
* updated REST API and MQTT (both of them use the same functionality)
* added ESP-heap information as MQTT message
* changed output name of automatic development build to fixed name (to have a static link from https://ahoydtu.de)
* updated user manual to latest MQTT and API changes

## 0.5.42
* fix web logout (auto logout)
* switched MQTT library
