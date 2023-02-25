# Changelog

(starting from release version `0.5.66`)

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
