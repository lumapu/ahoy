# Development Changes

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
