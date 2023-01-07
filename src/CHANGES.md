# Changelog

(starting from release version `0.5.66`)

## 0.5.70

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
