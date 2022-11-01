# PCB for Wemos D1 + Nokia5110-display + nRF24L01+

Simple pcb to plug a nRF24L01+ together with a Wemos D1 Mini / Pro and a NOKIA5110-display (known as PCD8544).
One goal :  You can power this pcb via USB-interface of D1-mini.

attached zip was created with easyeda. You can order easyly on jlcpcb.com.

## Attention
Be sure you have the right type of nokia-display-pcb !
Check pin-header-description (There are different versions avail)!
( check picture : LIGHT (or BL) is placed between GND and VCC )

![img](Nokia5110-LCD.jpg) or ![img](Nokia5110-LCD2.jpg)

---

A picture of earlier version of pcb.

![img](PCB-V1.jpg)

D1-mini and NRF placed on front side, nokia display will soldered bottom.
You can use this PCB also without mounting a Nokia-display.

Actual version of PCB has mounting holes and additional 5V/Gnd -pin access.

## after soldering ...

![img](1_Front.jpg)
NRF & Wemos on Front

![img](2_Bottom.jpg)
Nokia Display soldered on bottom side

![img](3_booting.jpg)
while booting (wait for wifi, 1st data, ...)

![img](4_runIP.jpg)
1st screen show most interesting info and IP in bottom line

![img](5_runTime.jpg)
this is the normal screen with time in bottom line (refresh all 5 seconds)
