Changelog for ahoy-all-in-one compared to 0.6.9 of the main project

- configurable read SML/OBIS from UART (stream parser with min resources needed); Connections 9600,8,n,1, GND-GND, VCC-3V3, TX-TX, RX-RX
- prepared to show chart of grid power and total solar ac power for current days daylight period (6 a.m. to 8 p.m.)
- show current grid power
- show max solar ac/dc power
- improved radio retransmit (complete retransmit if nothing was received, but only when inverter ought to be active) for the hm series)
- Heuristic for choosing the best send channel (of 5 possible) helps reducing retransmits
- optimized receiving handler for HM Inverters with 1 DC input (reduced frequency hopping and reduced answer timeout)
- shortcut radio traces a little bit

DRAWBACKS:
- ESP8266: MQTT Source is commented out (except 1 var which is used for other purpose as well)
- ESP8266: only up to 2 Inverters are supported (was: 10), for ESP32: 16
- RX/TX of UART0 is used for serial interface to IR sensor. Of course you cannot operate a display that uses RX/TX pins of UART0, simultaneously. And unplug serial connection bevor updating via USB (see also below)
- Due to a non-matching licence model of the charting lib certain parts of visualization.html are commented out. See comments there.
- reduced platforms: only 8266 and ESP32 S3 (opendtufusionv1) currently

But: Currently there is enough heap available for stable operation on a ESP8266 plattform (WEMOS D1 R1). So adjust the number of inverters and enable MQTT to your needs and see if the AHOY-DTU is still stable in operation with your hw plattform.
To update firmware via USB, unplug serial connection to IR sensor first. Surprisingly during normal operation it seems that one can use a fully connected USB cable (for power supply). But I'm not sure if this works for all hardware plattforms.

