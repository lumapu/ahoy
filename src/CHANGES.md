# Development Changes

## 0.8.135 - 2024-08-11
* translated `/system` #1717

## 0.8.134 - 2024-08-10
* combined Ethernet and WiFi variants - Ethernet is now always included, but needs to be enabled if needed
* improved statistic data in `/system`
* redesigned `/system`

## 0.8.133 - 2024-08-10
* Ethernet variants now support WiFi as fall back / configuration

## 0.8.132 - 2024-08-09
* fix boot loop once no ePaper is connected #1713, #1714
* improved refresh routine of ePeper
* added default pin seetings for opendtufusion board

## 0.8.131 - 2024-08-08
* improved refresh routine of ePaper, full refresh each 12h #1107 #1706

## 0.8.130 - 2024-08-04
* fix message `ERR_DUPLICATE_INVERTER` #1705, #1700
* merge PR: Power limit command accelerated #1704
* merge PR: reduce update cycle of ePaper from 5 to 10 seconds #1706
* merge PR: small fixes in different files #1711
* add timestamp to JSON output #1707
* restart Ahoy using MqTT #1667

## 0.8.129 - 2024-07-11
* sort alarms ascending #1471
* fix alarm counter for first alarm
* prevent add inverter multiple times #1700

## 0.8.128 - 2024-07-10
* add environments for 16MB flash size ESP32-S3 aka opendtufusion
* prevent duplicate alarms, update end time once it is received #1471

## 0.8.127 - 2024-06-21
* add grid file #1677
* merge PR: Bugfix Inv delete not working with password protection #1678

## 0.8.126 - 2024-06-12
* merge PR: Update pubMqtt.h - Bugfix #1673 #1674

## 0.8.125 - 2024-06-09
* fix ESP8266 compilation
* merge PR: active_PowerLimit #1663

## 0.8.124 - 2024-06-06
* improved MqTT `OnMessage` (threadsafe)
* support of HERF inverters, serial number is converted in Javascript #1425
* revert buffer size in `RestAPI` for ESP8266 #1650

## 0.8.123 - 2024-05-30
* fix ESP8266, ESP32 static IP #1643 #1608
* update MqTT library which enhances stability #1646
* merge PR: MqTT JSON Payload pro Kanal und total, auswählbar #1541
* add option to publish MqTT as json
* publish rssi not on ch0 any more, published on `topic/rssi`
* add total power to index page (if multiple inverters are configured)
* show device name in html title #1639
* update AsyncWebserver library to `3.2.2`
* add environment name to filename of coredump

## 0.8.122 - 2024-05-23
* add button for donwloading coredump (ESP32 variants only)

## 0.8.121 - 2024-05-20
* fix ESP32 factory image generation
* fix plot of history graph #1635

## 0.8.120 - 2024-05-18
* fix crash if invalid serial number was set -> inverter will be disabled automatically
* improved and fixed factory image generation
* fix HMT-1800-4T number of inputs #1628

## 0.8.119 - 2024-05-17
* fix reset values at midnight if WiFi isn't available #1620
* fix typo in English versions
* add yield day to history graph #1614
* added script and [instructions](../manual/factory_firmware.md) how to generate factory firmware which includes predefined settings
* merge PR: Fix MI overnight behaviour #1626

## 0.8.118 - 2024-05-10
* possible fix reset max values #1609
* slightly improved WiFi reconnect
* update AsyncWebserver to `3.2.0`

## 0.8.117 - 2024-05-09
* fix reboot issue #1607 #1606
* fix max temperature tooltip if only one inverter is configured #1605

## 0.8.116 - 2024-05-05
* calculation of max AC power
* fix counter overflow communication queue
* added max inverter temperature

## 0.8.115 - 2024-05-03
* fix inverter communication with manual time sync #1603
* improved queue, only add new object once they not exist in queue
* added option to reset values on communication start (sunrise)
* fixed calculation of max AC power (API, MqTT)

## 0.8.114 - 2024-04-29
* fix ESP8266 compile
* fix history graph
* fix close button color of modal windows in dark mode #1598
* fix only one total field in `/live` #1579

## 0.8.113 - 2024-04-25
* code cleanup
* fix ESP32-C3 compile

## 0.8.112 - 2024-04-24
* improved wizard
* converted ePaper and Ethernet to hal-SPI
* improved network connection

## 0.8.111 - 2024-04-17
* fix MqTT discovery field `ALARM_MES_ID` #1591
* fix Wifi reconnect for ESP32 #1589 #1575
* open link from `index.html` in new tab #1588 #1587
* merge PR: Disable upload and import buttons when no file is selected #1586 #1519

## 0.8.110 - 2024-04-11
* revert CMT2300A changes #1553
* merged PR: fix closing tag #1584
* add disable retain flag #1582
* fix German translation #1569
* improved `Wizard`

## 0.8.109 - 2024-04-09
* fix hal patch

## 0.8.108 - 2024-04-09
* point to git SHA for `NRF24` library

## 0.8.107 - 2024-04-08
* fix boot loop on `reboot on midnight` feature #1542, #1599, #1566, #1571
* fix German translation #1569
* improved `Wizard`

## 0.8.106 - 2024-04-05
* fix bootloop with CMT and NRF on ESP32 #1566 #1562
* possible fix of #1553
* change MqTT return value of power limit acknowledge from `boolean` to `float`. The value returned is the same as it was set to confirm reception (not the read back value)

## 0.8.105 - 2024-04-05
* cleanup of `defines.h`
* fix compile of esp32-minimal

## 0.8.104 - 2024-04-04
* fix reboot on inverter save (ESP32) #1559
* fix NRF and Ethernet #1506

## 0.8.103 - 2024-04-02
* merge PR: fix: get refresh property from object #1552
* merge PR: fix typos and spelling in Github Issue template #1550
* merge PR: shorten last cmt waiting time #1549
* fix cppcheck warnings
* changed MqTT retained flags of some topics

## 0.8.102 - 2024-04-01
* fix NTP for `opendtufusion` #1542
* fix scan WiFi in AP mode
* fix MDNS #1538
* improved Wizard
* improved MqTT on devcontrol e.g. set power limit

## 0.8.101 - 2024-03-28
* updated converter scripts to include all enabled features again (redundant scan of build flags) #1534

## 0.8.100 - 2024-03-27
* fix captions in `/history #1532
* fix get NTP time #1529 #1530
* fix translation #1516

## 0.8.99 - 2024-03-27
* fix compilation of all environments

## 0.8.98 - 2024-03-24
* new network routines

## 0.8.97 - 2024-03-22
* add support for newest generation of inverters with A-F in their serial number
* fix NTP and sunrise / sunset
* set default coordinates to the mid of Germany #1516

## 0.8.96 - 2024-03-21
* fix precision of power limit in `/live` #1517
* fix translation of `Werte ausgeben` in `settings` #1507
* add grid profile #1518

## 0.8.95 - 2024-03-17
* fix NTP issues #1440 #1497 #1499

## 0.8.94 - 2024-03-16
* switched AsyncWebServer library
* Ethernet version now uses same AsyncWebServer library as Wifi version
* fix translation of `/history`
* fix RSSI on `/history` #1463

## 0.8.93 - 2024-03-14
* improved history graph in WebUI #1491
* merge PR: 1491

## 0.8.92 - 2024-03-10
* fix read back of limit value, now with one decimal place
* added grid profile for Mexico #1493
* added language to display on compile time #1484, #1255, #1479
* added new environment `esp8266-all` which replace the original `esp8266`. The original now only have `MqTT` support but `Display` and `History` plugins are not included any more #1451

## 0.8.91 - 2024-03-05
* fix javascript issues #1480

## 0.8.90 - 2024-03-05
* added preprocessor defines to HTML (from platform.ini) to reduce the HTML in size if modules aren't enabled
* auto build minimal English versions of ESP8266 and ESP32

## 0.8.89 - 2024-03-02
* merge PR: Collection of small fixes #1465
* fix: show esp type on `/history` #1463
* improved HMS-400-1T support (serial number 1125...) #1460

## 0.8.88 - 2024-02-28
* fix MqTT statistic data overflow #1458
* add HMS-400-1T support (serial number 1125...) #1460
* removed `yield efficiency` because the inverter already calculates correct #1243
* merge PR: Remove hint to INV_RESET_MIDNIGHT resp. INV_PAUSE_DURING_NIGHT #1431

## 0.8.87 - 2024-02-25
* fix translations #1455 #1442

## 0.8.86 - 2024-02-23
* RestAPI check for parent element to be JsonObject #1449
* fix translation #1448 #1442
* fix reset values when inverter status is 'not available' #1035 #1437

## 0.8.85 - 2024-02-22
* possible fix of MqTT fix "total values are sent to often" #1421
* fix translation #1442
* availability check only related to live data #1035 #1437

## 0.8.84 - 2024-02-19
* fix homeassistant autodiscovery #1432
* merge PR: more gracefull handling of complete retransmits #1433

# RELEASE 0.8.83 - 2024-02-16

## 0.8.82 - 2024-02-15
* fixed crash once firmware version was read and sent via MqTT #1428
* possible fix: reset yield offset on midnight #1429

## 0.8.81 - 2024-02-13
* fixed authentication with empty token #1415
* added new setting for future function to send log via MqTT
* combined firmware and hardware version to JSON topics (MqTT) #1212

## 0.8.80 - 2024-02-12
* optimize API authentication, Error-Codes #1415
* breaking change: authentication API command changed #1415
* breaking change: limit has to be send als `float`, `0.0 .. 100.0` #1415
* updated documentation #1415
* fix don't send control command twice #1426

## 0.8.79 - 2024-02-11
* fix `opendtufusion` build (started only once USB-console was connected)
* code quality improvments

## 0.8.78 - 2024-02-10
* finalized API token access #1415
* possible fix of MqTT fix "total values are sent to often" #1421
* removed `switchCycle` from `hmsRadio.h` #1412
* merge PR: Add hint to INV_RESET_MIDNIGHT resp. INV_PAUSE_DURING_NIGHT #1418
* merge PR: simplify rxOffset logic #1417
* code quality improvments

## 0.8.77 - 2024-02-08
* merge PR: BugFix: ACK #1414
* fix suspicious if condition #1416
* prepared API token for access, not functional #1415

## 0.8.76 - 2024-02-07
* revert changes from yesterday regarding snprintf and its size #1410, #1411
* reduced cppcheck linter warnings significantly
* try to improve ePaper (ghosting) #1107

## 0.8.75 - 2024-02-06
* fix active power control value #1406, #1409
* update Mqtt lib to version `1.6.0`
* take care of null terminator of chars #1410, #1411

## 0.8.74 - 2024-02-05
* reduced cppcheck linter warnings significantly

## 0.8.73 - 2024-02-03
* fix nullpointer during communication #1401
* added `max_power` to MqTT total values #1375

## 0.8.72 - 2024-02-03
* fixed translation #1403
* fixed sending commands to inverters which are soft turned off #1397
* reduce switchChannel command for HMS (only each 5th cycle it will be send now)

## 0.8.71 - 2024-02-03
* fix heuristics reset
* fix CMT missing frames problem
* removed inverter gap setting
* removed add to total (MqTT) inverter setting
* fixed sending commands to inverters which are soft turned off
* save settings before they are exported #1395
* fix autologin bug if no password is set
* translated `/serial`
* removed "yield day" history

## 0.8.70 - 2024-02-01
* prevent sending commands to inverter which isn't active #1387
* protect commands from popup in `/live` if password is set #1199

## 0.8.69 - 2024-01-31
* merge PR: Dynamic retries, pendular first rx chan #1394

## 0.8.68 - 2024-01-29
* fix HMS / HMT startup
* added `flush_rx` to NRF on TX
* start with heuristics set to `0`
* added warning for WiFi channel 12-14 (ESP8266 only) #1381

## 0.8.67 - 2024-01-29
* fix HMS frequency
* fix display of inverter id in serial log (was displayed twice)

## 0.8.66 - 2024-01-28
* added support for other regions - untested #1271
* fix generation of DTU-ID; was computed twice without reset if two radios are enabled

## 0.8.65 - 2024-01-24
* removed patch for NRF `PLOS`
* fix lang issues #1388
* fix build on Windows of `opendtufusion` environments (git: trailing whitespaces)

## 0.8.64 - 2024-01-22
* add `ARC` to log (NRF24 Debug)
* merge PR: ETH NTP update bugfix #1385

## 0.8.63 - 2024-01-22
* made code review
* fixed endless loop #1387

## 0.8.62 - 2024-01-21
* updated version in footer #1381
* repaired radio statistics #1382

## 0.8.61 - 2024-01-21
* add favicon to header
* improved NRF communication
* merge PR: provide localized times to display mono classes #1376
* merge PR: Bypass OOM-Crash on minimal version & history access #1378
* merge PR: Add some REST Api Endpoints to avail_endpoints #1380

## 0.8.60 - 2024-01-20
* merge PR: non blocking nRF loop #1371
* merge PR: fixed millis in serial log #1373
* merge PR: fix powergraph scale #1374
* changed inverter gap to `1` as default (old settings will be overridden)

## 0.8.59 - 2024-01-18
* merge PR: solve display settings dependencies #1369
* fix language typos #1346
* full update of ePaper after booting #1107
* fix MqTT yield day reset even if `pause inverter during nighttime` isn't active #1368

## 0.8.58 - 2024-01-17
* fix missing refresh URL #1366
* fix view of grid profile #1365
* fix webUI translation #1346
* fix protection mask #1352
* merge PR: Add Watchdog for ESP32 #1367
* merge PR: ETH support for CMT2300A - HMS/HMT #1356
* full refresh of ePaper after booting #1107
* add optional custom link #1199
* pinout has an own subgroup in `/settings`
* grid profile will be displayed as hex in every case #1199

## 0.8.57 - 2024-01-15
* merge PR: fix immediate clearing of display after sunset #1364
* merge PR: MI-MQTT and last retransmit #1363
* fixed DTU-ID, now built from the unique part of the MAC
* fix lang in `/system` #1346
* added protection to prevent update to wrong firmware (environment check)

## 0.8.56 - 2024-01-15
* potential fix of update problems and random reboots #1359 #1354

## 0.8.55 - 2024-01-14
* merge PR: fix reboot problem with deactivated power graph #1360
* changed scope of variables and member functions inside display classes
* removed automatically "minimal" builds
* fix include of "settings.h" (was already done in #1360)
* merge PR: Enhancement: Add info about compiled modules to version string #1357
* add info about installed binary to `/update` #1353
* fix lang in `/system` #1346

## 0.8.54 - 2024-01-13
* added minimal version (without: MqTT, Display, History), WebUI is not changed!
* added simulator (must be activated before compile, standard: off)
* changed communication attempts back to 5

## 0.8.53 - 2024-01-12
* fix history graph
* fix MqTT yield day #1331

## 0.8.52 - 2024-01-11
* possible fix of 'division by zero' #1345
* fix lang #1348 #1346
* fix timestamp `max AC power` #1324
* fix stylesheet overlay `max AC power` #1324
* fix download link #1340
* fix history graph
* try to fix #1331

## 0.8.51 - 2024-01-10
* fix translation #1346
* further improve sending active power control command faster #1332
* added history protection mask
* merge PR: display graph improvements #1347

## 0.8.50 - 2024-01-09
* merge PR: added history charts to web #1336
* merge PR: small display changes #1339
* merge PR: MI - add "get loss logic" #1341
* translated `/history`
* fix translations in title of documents
* added translations for error messages #1343

## 0.8.49 - 2024-01-08
* fix send total values if inverter state is different from `OFF` #1331
* fix german language issues #1335

## 0.8.48 - 2024-01-07
* merge PR: pin selection for ESP-32 S2 #1334
* merge PR: enhancement: power graph display option #1330

## 0.8.47 - 2024-01-06
* reduce GxEPD2 lib to compile faster
* upgraded GxEPD2 lib to `1.5.3`
* updated espressif32 platform to `6.5.0`
* updated U8g2 to `2.35.9`
* started to convert deprecated functions of new ArduinoJson `7.0.0`
* started to have german translations of all variants (environments) #925 #1199
* merge PR: add defines for retry attempts #1329

## 0.8.46 - 2024-01-06
* improved communication

## 0.8.45 - 2024-01-05
* fix MqTT total values #1326
* start implementing a wizard for initial (WiFi) configuration #1199

## 0.8.44 - 2024-01-05
* fix MqTT transmission of data #1326
* live data is read much earlier / faster and more often #1272

## 0.8.43 - 2024-01-04
* fix display of sunrise in `/system` #1308
* fix overflow of `getLossRate` calculation #1318
* improved MqTT by marking sent data and improved `last_success` resends #1319
* added timestamp for `max ac power` as tooltip #1324 #1123 #1199
* repaired Power-limit acknowledge #1322
* fix `max_power` in `/visualization` was set to `0` after sunset

## 0.8.42 - 2024-01-02
* add LED to display whether it's night time or not. Can be reused as output to control battery system #1308
* merge PR: beautifiying typography, added spaces between value and unit for `/visualization` #1314
* merge PR: Prometheus add `getLossRate` and bugfixing #1315
* add loss rate to `/visualization` in the statistics window
* corrected `getLossRate` infos for MqTT and prometheus
* added information about working IRQ for NRF24 and CMT2300A to `/system`

## 0.8.41 - 2024-01-02
* fix display timeout (OLED) to 60s
* change offs to signed value

## 0.8.40 - 2024-01-02
* fix display of sunrise and sunset in `/system` #1308
* fix MqTT set power limit #1313

## 0.8.39 - 2024-01-01
* fix MqTT dis_night_comm in the morning #1309 #1286
* separated offset for sunrise and sunset #1308
* powerlimit (active power control) now has one decimal place (MqTT / API) #1199
* merge Prometheus metrics fix #1310
* merge MI grid profile request #1306
* merge update documentation / readme #1305
* add `getLossRate` to radio statistics and to MqTT #1199

## 0.8.38 - 2023-12-31
* fix Grid-Profile JSON #1304

## 0.8.37 - 2023-12-30
* added grid profiles
* format version of grid profile

# RELEASE 0.8.36 - 2023-12-30

## 0.8.35 - 2023-12-30
* added dim option for LEDS
* changed reload time for opendtufusion after update to 5s
* fix default interval and gap for communication
* fix serial number in exported json (was decimal, now correct as hexdecimal number)
* beautified factory reset
* added second stage for erase settings
* increased maximal number of inverters to 32 for opendtufusion board (ESP32-S3)
* fixed crash if CMT inverter is enabled, but CMT isn't configured

# RELEASE 0.8.34 - 2023-12-29

## 0.8.33 - 2023-12-29
* improved communication thx @rejoe2

## 0.8.32 - 2023-12-29
* fix `start` / `stop` / `restart` commands #1287
* added message, if profile was not read until now #1300
* added developer option to use 'syslog-server' instead of 'web-serail' #1299

## 0.8.31 - 2023-12-29
* added class to handle timeouts PR #1298

## 0.8.30 - 2023-12-28
* added info if grid profile was not found
* merge PR #1293
* merge PR #1295 fix ESP8266 pin settings
* merge PR #1297 fix layout for OLED displays

## 0.8.29 - 2023-12-27
* fix MqTT generic topic `comm_disabled` #1265 #1286
* potential fix of #1285 (reset yield day)
* fix fraction of yield correction #1280
* fix crash if `getLossRate` was read from inverter #1288 #1290
* reduce reload time for opendtufusion ethernet variant to 5 seconds
* added basic grid parser
* added ESP32-C3 mini environment #1289

## 0.8.28 - 2023-12-23
* fix bug heuristic
* add version information to clipboard once 'copy' was clicked
* add get loss rate @rejoe2
* improve communication @rejoe2

## 0.8.27 - 2023-12-18
* fix set power limit #1276

## 0.8.26 - 2023-12-17
* read grid profile as HEX (`live` -> click inverter name -> `show grid profile`)

## 0.8.25 - 2023-12-17
* RX channel ID starts with fixed value #1277
* fix static IP for Ethernet

## 0.8.24 - 2023-12-16
* fix NRF communication for opendtufusion ethernet variant

## 0.8.23 - 2023-12-14
* heuristics fix #1269 #1270
* moved `sendInterval` in settings, **important:** *will be reseted to 15s after update to this version*
* try to prevent access to radio classes if they are not activated
* fixed millis in serial log
* changed 'print whole trace' = `false` as default
* added communication loop duration in [ms] to serial console
* don't print Hex-Payload if 'print whole trace' == `false`

## 0.8.22 - 2023-12-13
* fix communication state-machine regarding zero export #1267

## 0.8.21 - 2023-12-12
* fix ethernet save inverter parameters #886
* fix ethernet OTA update #886
* improved radio statistics, fixed heuristic output for HMS and HMT inverters

## 0.8.20 - 2023-12-12
* improved HM communication #1259 #1249
* fix `loadDefaults` for ethernet builds #1263
* don't loop through radios which aren't in use #1264

## 0.8.19 - 2023-12-11
* added ms to serial log
* added (debug) option to configure gap between inverter requests

## 0.8.18 - 2023-12-10
* copied even more from the original heuristic code #1259
* added mDNS support #1262

## 0.8.17 - 2023-12-10
* possible fix of NRF with opendtufusion (without ETH)
* small fix in heuristics (if conditions made assignment not comparisson)

## 0.8.16 - 2023-12-09
* fix crash if NRF is not enabled
* updated heuristic #1080 #1259
* fix compile opendtufusion fusion ethernet

## 0.8.15 - 2023-12-09
* added support for opendtufusion fusion ethernet shield #886
* fixed range of HMS / HMT frequencies to 863 to 870 MHz #1238
* changed `yield effiency` per default to `1.0` #1243
* small heuristics improvements #1258
* added class to combine inverter heuristics fields #1258

## 0.8.14 - 2023-12-07
* fixed decimal points for temperature (WebUI) PR #1254 #1251
* fixed inverter statemachine available state PR #1252 #1253
* fixed NTP update and sunrise calculation #1240 #886
* display improvments #1248 #1247
* fixed overflow in `hmRadio.h` #1244

## 0.8.13 - 2023-11-28
* merge PR #1239 symbolic layout for OLED 128x64 + motion senser functionality
* fix MqTT IP addr for ETH connections PR #1240
* added ethernet build for fusion board, not tested so far

## 0.8.12 - 2023-11-20
* added button `copy to clipboard` to `/serial`

## 0.8.11 - 2023-11-20
* improved communication, thx @rejoe2
* improved heuristics, thx @rejoe2, @Oberfritze
* added option to strip payload of frames to significant area

## 0.8.10 - 2023-11-19
* fix Mi and HM inverter communication #1235
* added privacy mode option #1211
* changed serial debug option to work without reboot

## 0.8.9 - 2023-11-19
* merged PR #1234
* added new alarm codes
* removed serial interval, was not in use anymore

## 0.8.8 - 2023-11-16
* fix ESP8266 save inverter #1232

## 0.8.7 - 2023-11-13
* fix ESP8266 inverter settings #1226
* send radio statistics via MqTT #1227
* made night communication inverter depended
* added option to prevent adding values of inverter to total values (MqTT only) #1199

## 0.8.6 - 2023-11-12
* merged PR #1225
* improved heuristics (prevent update of statitistic during testing)

## 0.8.5 - 2023-11-12
* fixed endless loop while switching CMT frequency
* removed obsolete "retries" field from settings #1224
* fixed crash while defining new invertes #1224
* fixed default frequency settings
* added default input power to `400` while adding new inverters
* fixed color of wifi RSSI icon #1224

## 0.8.4 - 2023-11-10
* changed MqTT alarm topic, removed retained flag #1212
* reduce last_success MQTT messages (#1124)
* introduced tabs in WebGUI (inverter settings)
* added inverter-wise power level and frequency

## 0.8.3 - 2023-11-09
* fix yield day reset during day #848
* add total AC Max Power to WebUI
* fix opendtufusion build (GxEPD patch)
* fix null ptr PR #1222

## 0.8.2 - 2023-11-08
* beautified inverter settings in `setup` (preperation for future, settings become more inverter dependent)

## 0.8.1 - 2023-11-05
* added tx channel heuristics (per inverter)
* fix statistics counter

## 0.8.0 - 2023-10-??
* switched to new communication scheme

## 0.7.66 - 2023-10-04
* prepared PA-Level for CMT
* removed settings for number of retransmits, its fixed to `5` now
* added parentheses to have a excactly defined behaviour

## 0.7.65 - 2023-10-02
* MI control command review #1197

## 0.7.64 - 2023-10-02
* moved active power control to modal in `live` view (per inverter) by click on current APC state

## 0.7.63 - 2023-10-01
* fix NRF24 communication #1200

## 0.7.62 - 2023-10-01
* fix communication to inverters #1198
* add timeout before payload is tried to process (necessary for HMS/HMT)

## 0.7.61 - 2023-10-01
* merged `hmPayload` and `hmsPayload` into single class
* merged generic radio functions into new parent class `radio.h`
* moved radio statistics into the inverter - each inverter has now separate statistics which can be accessed by click on the footer in `/live`
* fix compiler warnings #1191
* fix ePaper logo during night time #1151

## 0.7.60 - 2023-09-27
* fixed typos in changelog #1172
* fixed MqTT manual clientId storage #1174
* fixed inverter name length in setup #1181
* added inverter name to the header of alarm list #1181
* improved code to avoid warning during compilation #1182
* fix scheduler #1188, #1179

## 0.7.59 - 2023-09-20
* re-add another HM-400 hardware serial number accidentally removed with `0.7.45` (#1169)
* merge PR #1170
* reduce last_success MQTT messages (#1124)
* add re-request if inverter is known to be online and first try fails
* add alarm reporting to MI (might need review!)
* rebuild MI limiting code closer to DTUSimMI example
* round APC in `W` to an integer #1171

## 0.7.58
* fix ESP8266 save settings issue #1166

## 0.7.57 - 2023-09-18
* fix Alarms are always in queue (since 0.7.56)
* fix display active power control to long for small devices #1165

## 0.7.56 - 2023-09-17
* only request alarms which were not received before #1113
* added flag if alarm was requested but not received and re-request it #1105
* merge PR #1163

## 0.7.55 - 2023-09-17
* fix prometheus builds
* fix ESP32 default pinout #1159
* added `opendtufusion-dev` because of annoying `-DARDUINO_USB_CDC_ON_BOOT=1` flag
* fix display of current power on `index`
* fix OTA, was damaged by version `0.7.51`, need to use webinstaller (from `0.7.51` to `0.7.54`)

## 0.7.54 - 2023-09-16
* added active power control in `W` to live view #201, #673
* updated docu, active power control related #706
* added current AC-Power to `index` page and removed version #763
* improved statistic data, moved to entire struct
* removed `/api/statistics` endpoint from REST-API

## 0.7.53 - 2023-09-16
* fix ePaper / display night behaviour #1151
* fix ESP8266 compile error

## 0.7.52 - 2023-09-16
* fix CMT configurable pins #1150, #1159
* update MqTT lib to version `1.4.5`

## 0.7.51 - 2023-09-16
* fix CMT configurable pins #1150
* fix default CMT pins for opendtufusion
* beautified `system`
* changed main loops, fix resets #1125, #1135

## 0.7.50 - 2023-09-12
* moved MqTT info to `system`
* added CMT info for ESP32 devices
* improved CMT settings, now `SCLK` and `SDIO` are configurable #1046, #1150
* changed `Power-Limit` in live-view to `Active Power Control`
* increase length of update file selector #1132

## 0.7.49 - 2023-09-11
* merge PR: symbolic icons for mono displays, PR #1136
* merge MI code restructuring PR #1145
* merge Prometheus PR #1148
* add option to strip webUI for ESP8266 (reduce code size, add ESP32 special features; `IF_ESP32` directives)
* started to get CMT info into `system` - not finished

## 0.7.48 - 2023-09-10
* fix SSD1309 2.42" display pinout
* improved setup page: save and delete of inverters

## 0.7.47 - 2023-09-07
* fix boot loop #1140
* fix regex in `setup` page
* fix MI serial number display `max-module-power` in `setup` #1142
* renamed `opendtufusionv1` to `opendtufusion`

## 0.7.46 - 2023-09-04
* removed `delay` from ePaper
* started improvements of `/system`
* fix LEDs to check all configured inverters
* send loop skip disabled inverters fix
* print generated DTU SN to console
* HW Versions for MI series PR #1133
* 2.42" display (SSD1309) integration PR #1139
* update user manual PR #1121
* add / rename alarm codes PR #1118
* revert default pin ESP32 for NRF23-CE #1132
* luminance of display can be changed during runtime #1106

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
* added old Changelog Entries, to have full log of changes

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
* fix MI crashes
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
* attempt to improve speed / response times (Schwuppdizitaet) #1075

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
* fix MqTT YieldDay Total goes to 0 several times #1016

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
* added option to adjust efficiency for yield (day/total) #1028

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
* fixed MqTT publish while applying power limit #1013
* slightly improved HMT live view (Voltage & Current)

## 0.7.8 - 2023-07-05
* fix `YieldDay`, `YieldTotal` and `P_AC` in `TotalValues` #929
* fix some serial debug prints
* merge PR #1005 which fixes issue #889
* merge homeassistant PR #963
* merge PR #890 which gives option for scheduled reboot at midnight (default off)

## 0.7.7 - 2023-07-03
* attempt to fix MqTT `YieldDay` in `TotalValues` #927
* attempt to fix MqTT `YieldDay` and `YieldTotal` even if inverters are not completely available #929
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
* support `.` and `,` as floating point separator in setup #881

## 0.6.6 - 2023-04-12
* increased distance for `import` button in mobile view #879
* changed `led_high_active` to `bool` #879

## 0.6.5 - 2023-04-11
* fix #845 MqTT subscription for `ctrl/power/[IV-ID]` was missing
* merge PR #876, check JSON settings during read for existence
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
* merge LED fix - LED1 shows MqTT state, LED configurable active high/low #839
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
* merge: PR SPI pins configurable (ESP32) #807, #806 (requires manual set of MISO=19, MOSI=23, SCLK=18 in GUI for existing installs)
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
* merged #742 MI Improvements
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
* added part of mac address to MQTT client ID to separate multiple ESPs in same network
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
* re-enabled FlashStringHelper because of lacking RAM
* complete rewrite of monochrome display class, thx to @dAjaY85 -> displays are now configurable in setup
* fix power limit not possible #607

## 0.5.74
* improved payload handling (retransmit all fragments on CRC error)
* improved `isAvailable`, checks all record structs, inverter becomes available more early because version is check first
* fix tickers were not set if NTP is not available
* disabled annoying `FlashStringHelper` it gives randomly Exceptions during development, feels more stable since then
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
* Powerlimit is transferred immediately to inverter

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
* removed `tickSunset` and `tickSunrise` from MQTT. It's not needed any more because of minute wise check of status (`processIvStatus`)
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
* added communication enable / disable (to test multiple DTUs with the same inverter)
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
* increased number of allowed characters for MQTT user, broker and password, @DanielR92
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
* changed index.html interval to static 10 seconds
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

