Changelog for ahoy-all-in-one compared to 0.6.9 of the main project

- read SML/OBIS from UART (stream parser with min resources needed); Connections 9600,8,n,1, GND-GND, VCC-3V3, TX-TX, RX-RX
- prepared to show chart of grid power and total solar ac power for current days daylight period (6 a.m. to 8 p.m.)
- show current grid power
- show max solar ac/dc power
- improved radio retransmit (complete retransmit if nothing was received, but only when inverter ought to be active)
- shortcut radio traces a little bit

DRAWBACKS:
- MQTT Source is commented out (except 1 var which is used for other purpose as well)
- only up to 2 Inverters (was: 10)
- RX/TX of UART0 is used for serial interface to IR sensor.
But: Currently there is enough heap available for stable operation on a ESP8266 plattform (WEMOS D1 R1). So adjust to your needs and see if the AHOY-DTU is still stable in operation with your hw plattform.
To update firmware via USB, unplug serial connection to IR sensor first. Surprisingly during normal operation it seems that one can use a full connected USB cable (for power supply). But I'm not sure, if this allways will be true.
Of course you cannot operate a display that uses RX/TX pins of UART0, simultanously.
- Due to not matching licence of the chart lib certain parts of visualization.html are commented out. See comments there.
