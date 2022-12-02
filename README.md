![actions/workflows/compile_esp8266.yml](../../actions/workflows/compile_esp8266.yml/badge.svg) ![actions/workflows/compile_development.yml](../../actions/workflows/compile_development.yml/badge.svg)

# 🖐 Ahoy!
![Logo](https://github.com/grindylow/ahoy/blob/main/doc/logo1_small.png?raw=true)

**Communicate with Hoymiles inverters via radio**. Get actual values like power, current, daily energy and set parameters like the power limit via web interface or MQTT. In this repository you will find different approaches means Hardware / Software to realize the described functionalities.

List of approaches

- [ESP8266/ESP32, C++](tools/esp8266/) 👈 the most effort is spent here
- [Arduino Nano, C++](tools/nano/NRF24_SendRcv/) -> adapt. v0.5.17 to nano, serial IF only (AhoyUL) e.g. for FHEM, Python
- [Raspberry Pi, Python](tools/rpi/)
- [Others, C/C++](tools/nano/NRF24_SendRcv/)

## Quick Start with ESP8266
- [Go here ✨](tools/esp8266/README.md#things-needed)


## Success Stories
- [Getting the data into influxDB and visualize them in a Grafana Dashboard](https://grafana.com/grafana/dashboards/16850-pv-power-ahoy/) (thx @Carl)

## Support, Feedback, Information and Discussion
- [Discord Server (~ 300 Users)](https://discord.gg/WzhxEY62mB)
- [The root of development](https://www.mikrocontroller.net/topic/525778)

### Development
If you encounter issues use the issues here on github.

Please try to describe your issues as precise as possible and think about if this is a issue with the software here in the repository or other software components.

**Contributors are always welcome!**

### Related Projects
- [OpenDTU](https://github.com/tbnobody/OpenDTU)
  <- Our sister project ✨ for Hoymiles HM-300, HM-600, HM-1200 (for ESP32 only!)
- [DTU Simulator](https://github.com/Ziyatoe/DTUsimMI1x00-Hoymiles) 
  <- Go here ✨ for Hoymiles MI-300, MI-600, MI-1200 Software
