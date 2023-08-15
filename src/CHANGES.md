# Development Changes

## 0.7.34 - 2023-08-15
* fix MI chrashes
* fix some lost debug messages

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

## 0.7.26
* last Release
