# Development Changes

## 0.7.46 - 2023-09-04
* removed `delay` from ePaper
* started improvements of `/system`
* fix LEDs to check all configured inverters
* send loop skip disabled inverters fix
* print generated DTU SN to console
* HW Versions for MI series PR #1133
* 2.42" display (SSD1309) integration PR #1139

## 0.7.45 - 2023-08-29
* change ePaper text to symbols PR #1131
* added some invertes to dev info list #1111

## 0.7.44 - 2023-08-28
* fix `last_success` transmitted to often #1124

## 0.7.43 - 2023-08-28
* improved RSSI for NRF24, now it's read per package (and inverter) #1129
* arranged `heap` related info together in `/system`
* fix display navi during save
* clean up binary output, separated to folders

## 0.7.42 - 2023-08-27
* fix ePaper for opendtufusion_v2.x boards (Software SPI)
* add signal strength for NRF24 - PR #1119
* refactor wifi class to support ESP32 S2 PR #1127
* update platform for ESP32 to 6.3.2
* fix opendtufusion LED (were mixed)
* fix `last_success` transmitted to often #1124
* added ESP32-S3-mini to github actions
* added old Changlog Entries, to have full log of changes

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

# RELEASE 0.7.36 - 2023-08-18

## 0.7.35 - 2023-08-17
* fixed timestamp for alarms send over MqTT
* auto-patch of `AsyncWebServer` #834, #1036
* Update documentation in Git regarding `ESP8266` default NRF24 pin assignments

## 0.7.34 - 2023-08-16
* fixed timezone offset of alarms
* added `AC` and `DC` to `/live` #1098
* changed `ESP8266` default NRF24 pin assignments (`D3` = `CE` and `D4` = `IRQ`)
* fixed background of modal window for bright color
* fix MI chrashes
* fix some lost debug messages
* merged PR #1095, MI fixes for 0.7.x versions
* fix scheduled reboot #1097
* added vector graphic logo `/doc/logo.svg`
* merge PR #1093, improved Nokia5110 display layout

## 0.7.33 - 2023-08-15
* add alarms overview to WebGui #608
* fix webGui total values #1084

## 0.7.32 - 2023-08-14
* fix colors of live view #1091

## 0.7.31 - 2023-08-13
* fixed docu #1085
* changed active power limit MqTT messages to QOS2 #1072
* improved alarm messages, added alarm-id to log #1089
* trigger power limit read on next day (if inverter was offline meanwhile)
* disabled improv implementation to check if it is related to 'Schwuppdizitaet'
* changed live view to gray once inverter isn't available
* added inverter status to API
* changed sum of totals on WebGui depending on inverter status #1084
* merge maximum power (AC and DC) from PR #1080

## 0.7.30 - 2023-08-10
* attempt to improve speed / repsonse times (Schwuppdizitaet) #1075

## 0.7.29 - 2023-08-09
* MqTT alarm data was never sent, fixed
* REST API: added alarm data
* REST API: made get record obsolete
* REST API: added power limit acknowledge `/api/inverter/id/[0-x]` #1072

## 0.7.28 - 2023-08-08
* fix MI inverter support #1078

## 0.7.27 - 2023-08-08
* added compile option for ethernet #886
* fix ePaper configuration, missing `Busy`-Pin #1075

# RELEASE 0.7.26 - 2023-08-06

* fix MqTT `last_success`

# RELEASE 0.7.25 - 2023-08-06

## 0.7.24 - 2023-08-05
* merge PR #1069 make MqTT client ID configurable
* fix #1016, general MqTT status depending on inverter state machine
* changed icon for fully available inverter to a filled check mark #1070
* fixed `last_success` update with MqTT #1068
* removed `improv` esp-web-installer script, because it is not fully functional at this time

## 0.7.23 - 2023-08-04
* merge PR #1056, visualization html
* update MqTT library to 1.4.4
* update RF24 library to 1.4.7
* update ArduinoJson library to 6.21.3
* set minimum invervall for `/live` to 5 seconds

## 0.7.22 - 2023-08-04
* attempt to fix homeassistant auto discovery #1066

## 0.7.21 - 2023-07-30
* fix MqTT YieldDay Total goes to 0 serveral times #1016

## 0.7.20 - 2023-07-28
* merge PR #1048 version and hash in API, fixes #1045
* fix: no yield day update if yield day reads `0` after inverter reboot (mostly on evening) #848
* try to fix Wifi override #1047
* added information after NTP sync to WebUI #1040

## 0.7.19 - 2023-07-27
* next attempt to fix yield day for multiple inverters #1016
* reduced threshold for inverter state machine from 60min to 15min to go from state `WAS_ON` to `OFF`

## 0.7.18 - 2023-07-26
* next attempt to fix yield day for multiple inverters #1016

## 0.7.17 - 2023-07-25
* next attempt to fix yield day for multiple inverters #1016
* added two more states for the inverter status (also docu)

## 0.7.16 - 2023-07-24
* next attempt to fix yield day for multiple inverters #1016
* fix export settings date #1040
* fix time on WebUI (timezone was not observed) #913 #1016

## 0.7.15 - 2023-07-23
* add NTP sync interval #1019
* adjusted range of contrast / luminance setting #1041
* use only ISO time format in Web-UI #913

## 0.7.14 - 2023-07-23
* fix Contrast for Nokia Display #1041
* attempt to fix #1016 by improving inverter status
* added option to adjust effiency for yield (day/total) #1028

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

# RELEASE 0.6.9 - 2023-04-19

## 0.6.8 - 2023-04-19
* fix #892 `zeroYieldDay` loop was not applied to all channels

## 0.6.7 - 2023-04-13
* merge PR #883, improved store of settings and javascript, thx @tastendruecker123
* support `.` and `,` as floating point seperator in setup #881

## 0.6.6 - 2023-04-12
* increased distance for `import` button in mobile view #879
* changed `led_high_active` to `bool` #879

## 0.6.5 - 2023-04-11
* fix #845 MqTT subscription for `ctrl/power/[IV-ID]` was missing
* merge PR #876, check JSON settings during read for existance
* **NOTE:** incompatible change: renamed `led_high_active` to `act_high`, maybe setting must be changed after update
* merge PR #861 do not send channel metric if channel is disabled

## 0.6.4 - 2023-04-06
* merge PR #846, improved NRF24 communication and MI, thx @beegee3 & @rejoe2
* merge PR #859, fix burger menu height, thx @ThomasPohl

## 0.6.3 - 2023-04-04
* fix login, password length was not checked #852
* merge PR #854 optimize browser caching, thx @tastendruecker123 #828
* fix WiFi reconnect not working #851
* updated issue templates #822

## 0.6.2 - 2023-04-04
* fix login from multiple clients #819
* fix login screen on small displays

## 0.6.1 - 2023-04-01
* merge LED fix - LED1 shows MqTT state, LED configureable active high/low #839
* only publish new inverter data #826
* potential fix of WiFi hostname during boot up #752

# RELEASE 0.6.0 - 2023-03-27

## 0.5.110
* MQTT fix reconnection by new lib version #780
* add `about` page
* improved documentation regarding SPI pins #814
* improved documentation (getting started) #815 #816
* improved MI 4-ch inverter #820

## 0.5.109
* reduced heap fragmentation by optimizing MqTT #768
* ePaper: centered text thx @knickohr

## 0.5.108
* merge: PR SPI pins configureable (ESP32) #807, #806 (requires manual set of MISO=19, MOSI=23, SCLK=18 in GUI for existing installs)
* merge: PR MI serial outputs #809
* fix: no MQTT `total` sensor for autodiscover if only one inverter was found #805
* fix: MQTT `total` renamed to `device_name` + `_TOTOL` for better visibility #805

## 0.5.107
* fix: show save message
* fix: removed serial newline for `enqueueCmd`
* Merged improved Prometheus #808

## 0.5.106
* merged MI and debug message changes #804
* fixed MQTT autodiscover #794, #632

## 0.5.105
* merged MI, thx @rejoe2 #788
* fixed reboot message #793

## 0.5.104
* further improved save settings
* removed `#` character from ePaper
* fixed saving pinout for `Nokia-Display`
* removed `Reset` Pin for monochrome displays
* improved wifi connection #652

## 0.5.103
* merged MI improvements, thx @rejoe2 #778
* changed display inverter online message
* merged heap improvements #772

## 0.5.102
* Warning: old exports are not compatible any more!
* fix JSON import #775
* fix save settings, at least already stored settings are not lost #771
* further save settings improvements (only store inverters which are existing)
* improved display of settings save return value
* made save settings asynchronous (more heap memory is free)

## 0.5.101
* fix SSD1306
* update documentation
* Update miPayload.h
* Update README.md
* MI - remarks to user manual
* MI - fix AC calc
* MI - fix status msg. analysis

## 0.5.100
* fix add inverter `setup.html` #766
* fix MQTT retained flag for total values #726
* renamed buttons for import and export `setup.html`
* added serial message `settings saved`

## 0.5.99
* fix limit in [User_Manual.md](../User_Manual.md)
* changed `contrast` to `luminance` in `setup.html`
* try to fix SSD1306 display #759
* only show necessary display pins depending on setting

## 0.5.98
* fix SH1106 rotation and turn off during night #756
* removed MQTT subscription `sync_ntp`, `set_time` with a value of `0` does the same #696
* simplified MQTT subscription for `limit`. Check [User_Manual.md](../User_Manual.md) for new syntax #696, #713
* repaired inverter wise limit control
* fix upload settings #686

## 0.5.97
* Attention: re-ordered display types, check your settings! #746
* improved saving settings of display #747, #746
* disabled contrast for Nokia display #746
* added Prometheus as compile option #719, #615
* update MQTT lib to v1.4.1
* limit decimal places to 2 in `live`
* added `-DPIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48` to esp8266 debug build #657
* a `max-module-power` of `0` disables channel in live view `setup`
* merge MI improvements, get firmware information #753

## 0.5.96
* added Nokia display again for ESP8266 #764
* changed `var` / `VAr` to SI unit `var` #732
* fix MQTT retained flags for totals (P_AC, P_DC) #726, #721

## 0.5.95
* merged #742 MI Improvments
* merged #736 remove obsolete JSON Endpoint

## 0.5.94
* added ePaper (for ESP32 only!), thx @dAjaY85 #735
* improved `/live` margins #732
* renamed `var` to `VAr` #732

## 0.5.93
* improved web API for `live`
* added dark mode option
* converted all forms to reponsive design
* repaired menu with password protection #720, #716, #709
* merged MI series fixes #729

## 0.5.92
* fix mobile menu
* fix inverters in select `serial.html` #709

## 0.5.91
* improved html and navi, navi is visible even when API dies #660
* reduced maximum allowed JSON size for API to 6000Bytes #660
* small fix: output command at `prepareDevInformCmd` #692
* improved inverter handling #671

## 0.5.90
* merged PR #684, #698, #705
* webserial minor overflow fix #660
* web `index.html` improve version information #701
* fix MQTT sets power limit to zero (0) #692
* changed `reset at midnight` with timezone #697

## 0.5.89
* reduced heap fragmentation (removed `strtok` completely) #644, #645, #682
* added part of mac address to MQTT client ID to seperate multiple ESPs in same network
* added dictionary for MQTT to reduce heap-fragmentation
* removed `last Alarm` from Live view, because it showed always the same alarm - will change in future

## 0.5.88
* MQTT Yield Day zero, next try to fix #671, thx @beegee3
* added Solenso inverter to supported devices
* improved reconnection of MQTT #650

## 0.5.87
* fix yield total correction as module (inverter input) value #570
* reneabled instant start communication (once NTP is synced) #674

## 0.5.86
* prevent send devcontrol request during disabled night communication
* changed yield total correction as module (inverter input) value #570
* MQTT Yield Day zero, next try to fix #671

## 0.5.85
* fix power-limit was not checked for max retransmits #667
* fix blue LED lights up all the time #672
* fix installing schedulers if NTP server isn't available
* improved zero values on triggers #671
* hardcoded MQTT subtopics, because wildcard `#` leads to errors
* rephrased some messages on webif, thx to @Argafal #638
* fixed 'polling stop message' on `index.html` #639

## 0.5.84
* fix blue LED lights up all the time #672
* added an instant start communication (once NTP is synced)
* add MI 3rd generation inverters (10x2 serial numbers)
* first decodings of messages from MI 2nd generation inverters

## 0.5.83
* fix MQTT publishing, `callback` was set but reset by following `setup()`

## 0.5.82
* fixed communication error #652
* reset values is no bound to MQTT any more, setting moved to `inverter` #649
* fixed wording on `index.hmtl` #661

## 0.5.81
* started implementation of MI inverters (setup.html, own processing `MiPayload.h`)

## 0.5.80
* fixed communication #656

## 0.5.79
* fixed mixed reset flags #648
* fixed `mCbAlarm` if MQTT is not used #653
* fixed MQTT `autodiscover` #630 thanks to @antibill51
* next changes from @beegee many thanks for your contribution!
* replaced `CircularBuffer` by `std::queue`
* reworked `hmRadio.h` completely (interrupts, packaging)
* fix exception while `reboot`
* cleanup MQTT coding

## 0.5.78
* further improvements regarding wifi #611, fix connection if only one AP with same SSID is there
* fix endless loop in `zerovalues` #564
* fix auto discover again #565
* added total values to autodiscover #630
* improved zero at midnight #625

## 0.5.77
* fix wrong filename for automatically created manifest (online installer) #620
* added rotate display feature #619
* improved Prometheus endpoint #615, thx to @fsck-block
* improved wifi to connect always to strongest RSSI, thx to @beegee3 #611

## 0.5.76
* reduce MQTT retry interval from maximum speed to one second
* fixed homeassistant autodiscovery #565
* implemented `getNTPTime` improvements #609 partially #611
* added alarm messages to MQTT #177, #600, #608

## 0.5.75
* fix wakeup issue, once wifi was lost during night the communication didn't start in the morning
* reenabled FlashStringHelper because of lacking RAM
* complete rewrite of monochrome display class, thx to @dAjaY85 -> displays are now configurable in setup
* fix power limit not possible #607

## 0.5.74
* improved payload handling (retransmit all fragments on CRC error)
* improved `isAvailable`, checkes all record structs, inverter becomes available more early because version is check first
* fix tickers were not set if NTP is not available
* disabled annoying `FlashStringHelper` it gives randomly Expeptions during development, feels more stable since then
* moved erase button to the bottom in settings, not nice but more functional
* split `tx_count` to `tx_cnt` and `retransmits` in `system.html`
* fix mqtt retransmit IP address #602
* added debug infos for `scheduler` (web -> `/debug` as trigger prints list of tickers to serial console)

## 0.5.73
* improved payload handling (request / retransmit) #464
* included alarm ID parse to serial console (in development)

## 0.5.72
* repaired system, scheduler was not called any more #596

## 0.5.71
* improved wifi handling and tickers, many thanks to @beegee3 #571
* fixed YieldTotal correction calculation #589
* fixed serial output of power limit acknowledge #569
* reviewed `sendDiscoveryConfig` #565
* merged PR `Monodisplay`, many thanks to @dAjaY85 #566, Note: (settings are introduced but not able to be modified, will be included in next version)

## 0.5.70
* corrected MQTT `comm_disabled` #529
* fix Prometheus and JSON endpoints (`config_override.h`) #561
* publish MQTT with fixed interval even if inverter is not available #542
* added JSON settings upload. NOTE: settings JSON download changed, so only settings should be uploaded starting from version `0.5.70` #551
* MQTT topic and inverter name have more allowed characters: `[A-Za-z0-9./#$%&=+_-]+`, thx: @Mo Demman
* improved potential issue with `checkTicker`, thx @cbscpe
* MQTT option for reset values on midnight / not avail / communication stop #539
* small fix in `tickIVCommunication` #534
* add `YieldTotal` correction, eg. to have the option to zero at year start #512

## 0.5.69
* merged SH1106 1.3" Display, thx @dAjaY85
* added SH1106 to automatic build
* added IP address to MQTT (version, device and IP are retained and only transmitted once after boot) #556
* added `set_power_limit` acknowledge MQTT publish #553
* changed: version, device name are only published via MQTT once after boot
* added `Login` to menu if admin password is set #554
* added `development` to second changelog link in `index.html` #543
* added interval for MQTT (as option). With this settings MQTT live data is published in a fixed timing (only if inverter is available) #542, #523
* added MQTT `comm_disabled` #529
* changed name of binaries, moved GIT-Sha to the front #538

## 0.5.68
* repaired receive payload
* Powerlimit is transfered immediately to inverter

## 0.5.67
* changed calculation of start / stop communication to 1 min after last comm. stop #515
* moved payload send to `payload.h`, function `ivSend` #515
* payload: if last frame is missing, request all frames again

# RELEASE 0.5.66 - 2022-12-30

## 0.5.65
* wifi, code optimization #509

## 0.5.64
* channel name can use any character, not limited any more
* added `/` to MQTT topic and Inverter name
* trigger for `calcSunrise` is now using local time #515
* fix reconnect timeout for WiFi #509
* start AP only after boot, not on WiFi connection loss
* improved /system `free_heap` value (measured before JSON-tree is built)

## 0.5.63
* fix Update button protection (prevent double click #527)
* optimized scheduler #515 (thx @beegee3)
* potential fix of #526 duplicates in API `/api/record/live`
* added update information to `index.html`

## 0.5.62
* fix MQTT `status` update
* removed MQTT `available_text` (can be deducted from `available`)
* enhanced MQTT documentation in `User_Manual.md`
* remvoed `tickSunset` and `tickSunrise` from MQTT. It's not needed any more because of minute wise check of status (`processIvStatus`)
* changed MQTT topic `status` to nummeric value, check documentation in `User_Manual.md`
* fix regular expression of `setup.html` for inverter name and channel name

## 0.5.61
* fix #521 no reconnect at beginning of day
* added immediate (each minute) report of inverter status MQTT #522
* added protection mask to select which pages should be protected
* update of monochrome display, show values also if nothing changed

## 0.5.60
* added regex to inverter name and MQTT topic (setup.html)
* beautified serial.html
* added ticker for wifi loop #515

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

# RELEASE 0.5.41 - 2022-11-22

# RELEASE 0.5.28 - 2022-10-30

# RELEASE 0.5.17 - 2022-09-06

# RELEASE 0.5.16 - 2022-09-04

