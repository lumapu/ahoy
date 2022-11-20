# Changelog

* fix browser sync NTP button
* added login feature (protect web ui)
* added static IP option
* improved initial boot - don't connect to `YOUR_WIFI_SSID` any more, directly boot into AP mode
* added status LED support
* improved MQTT handling (boot, periodic updates, no zero values any more)
* replaced deprecated workflow functions
* refactored code to make it more clearly
* added scheduler to register functions which need to be run each second / minute / ...
* changed settings to littlefs (-> no currupt settings in future on memory layout changes)
* added a lot of system infos to `System` page for support
