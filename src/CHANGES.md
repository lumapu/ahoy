# Changelog v0.5.65

**Note:** Version `0.5.42` to `0.5.64` were development versions. Last release version before this release was `0.5.41`
Detailed change log (development changes): https://github.com/lumapu/ahoy/blob/945a671d27d10d0f7c175ebbf2fbb2806f9cd79a/src/CHANGES.md


* updated REST API and MQTT (both of them use the same functionality)
* improved stability
* Regular expressions for input fields which are used for MQTT to be compliant to MQTT
* WiFi optimization (AP Mode and STA in parallel, reconnect if local STA is unavailable)
* improved display of `/system`
* fix Update button protection (prevent double click #527)
* optimized scheduler #515
* fix of duplicates in API `/api/record/live` (#526)
* added update information to `index.html` (check for update with github.com)
* fix web logout (auto logout)
* switched MQTT library
* removed MQTT `available_text` (can be deducted from `available`)
* enhanced MQTT documentation in `User_Manual.md`
* changed MQTT topic `status` to nummeric value, check documentation in `User_Manual.md`
* added immediate (each minute) report of inverter status MQTT #522
* increased MQTT user, pwd and topic length to 64 characters + `\0`. (The string end `\0` reduces the available size by one) #516
* added disable night communication flag to MQTT #505
* added MQTT <TOPIC>/status to show status over all inverters
* added MQTT RX counter to index.html
* added protection mask to select which pages should be protected
* added monochrome display that show values also if nothing changed and in offline mode #498
* added icons to index.html, added WiFi-strength symbol on each page
* refactored communication offset (adjustable in minutes now)
* factory reset formats entire little fs
* renamed sunrise / sunset on index.html to start / stop communication
* fixed static IP save
* fix NTP with static IP
* all values are displayed on /live even if they are 0
* added NRF24 info to Systeminfo
* reordered enqueue commands after boot up to prevent same payload length for successive commands
