---
name: Report Ahoy
about: question about Ahoy
title: ''
labels: ''
assignees: ''

---

Hardware
Modelname: _
[ ] ESP8266
[ ] ESP32
[ ] Raspberry Pi

nRF24L01+ Module
[ ] nRF24L01+ you verified this is a Plus model capable of the required 256kBit/s mode
[ ] square dot indicates original Nordic Semicon chip 
[ ] round dot indicates copy-cat / counterfeit SI labs chip

Antenna:
[ ] circuit Board
[ ] external Antenna

Power Stabilization:
[ ]  100uF Electrolytic Capacitor 
connected between +3.3V and GND (Pin 1 & 2) of the NRF Module
[ ] Voltage stabilizing motherboard

Connection diagram:
[ ] Image of the your wiring attached
Connection diagram you used:
| nRF24L01+ Pin | ESP8266/32 GPIO |
| -----------------  | ---------------------- |
| Pin 1 GND []     |  GND                          |
| Pin 2 +3.3V       |  +3.3V                        |
| Pin 3 CE              | GPIO_2/_4 CE        |
| Pin 4 CSN          | GPIO15/_5 CS         |
| Pin 5 SCK           | GPIO14/18 SCLK    |
| Pin 6 MOSI        | GPIO13/23 MOSI   |
| Pin 7 MISO        | GPIO12/19 MISO   |
| Pin 8 IRQ            | GPIO_0/0 IRQ     |
Note: [] GND Pin 1 has a square mark on the nRF24L01+ module

Software
[ ] AhoyDTU
[ ] OpenDTU
Version: 
Github Hash: 

Build & Flash Method:
[ ] Platform IO
[ ] Arduino
[ ] ESP Tools
Version: 

Desktop OS:
[ ] Linux
[ ] Windows
[ ] Mac OS

Debugging:
[ ] USB Serial Log (attached)
[ ] Setup settings (use our templates ... to be added)
