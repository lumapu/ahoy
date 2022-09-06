# Changelog

- v0.5.17
    * Bug fix for 1 channel inverters (HM300, HM400) see #246
    * Bug fix for read back the active power limit from inverter #243 (before version 0.5.16 the reported limit was just a copy of the user set point, now it is the actual value which the inverter uses)
    * Update the [user manual](https://github.com/grindylow/ahoy/blob/main/tools/esp8266/User_Manual.md); added section aobut the published data on mqtt; section about zero export control; added section about code implementation command queue 
    * Added tx-Id number to packet payload struct. (eg. can be 0x95 or 0xD1) --> less messages fails and faster handling of changing power limit
