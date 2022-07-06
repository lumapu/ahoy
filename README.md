![Logo](https://github.com/grindylow/ahoy/blob/main/doc/logo1_small.png?raw=true)

# ahoy
Various tools, examples, and documentation for communicating with Hoymiles microinverters.

In particular:

* `doc/hoymiles-format-description.txt` is a [detailed description of the communications format](doc/hoymiles-format-description.md) and the history of this project
* `doc/getting-started-ESP8266.md` shows the [hardware setup for an ESP8266-based system](doc/getting-started-ESP8266.md)
* The `tools` folder contains various software tools for RaspberryPi, Arduino and ESP8266/ESP32:
  * A [version for ESP8266](tools/esp8266/) that includes an web interface ![](../../actions/workflows/compile_esp8266.yml/badge.svg)
  * A [version for Arduino Nano](tools/nano/NRF24_SendRcv/)
  * An [alternative Version of the above](tools/NRF24_SendRcv/)
  * A [different implementation](tools/HoyDtuSim/)
  * An [implementation for Raspberry Pi](tools/rpi/) that polls an inverter and archives results as log files/stdout as well as posting them to an MQTT broker.

Contributors are always welcome!
