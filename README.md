[![CC BY-NC-SA 4.0][cc-by-nc-sa-shield]][cc-by-nc-sa] [![Ahoy Dev Build][dev-action-badge]][dev-action-link]

This work is licensed under a
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: https://creativecommons.org/licenses/by-nc-sa/4.0/de/
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg

[dev-action-badge]: https://github.com/lumapu/ahoy/actions/workflows/compile_development.yml/badge.svg
[dev-action-link]: https://github.com/lumapu/ahoy/actions/workflows/compile_development.yml


# 🖐 Ahoy!
![Logo](https://github.com/grindylow/ahoy/blob/main/doc/logo1_small.png?raw=true)

**Communicate with Hoymiles inverters via radio**. Get actual values like power, current, daily energy and set parameters like the power limit via web interface or MQTT. In this repository you will find different approaches means Hardware / Software to realize the described functionalities.

List of approaches

- [ESP8266/ESP32, C++](Getting_Started.md) 👈 the most effort is spent here
- [Arduino Nano, C++](tools/nano/NRF24_SendRcv/)
- [Raspberry Pi, Python](tools/rpi/)
- [Others, C/C++](tools/nano/NRF24_SendRcv/)

## Quick Start with ESP8266
- [Go here ✨](Getting_Started.md#things-needed)
- [Our Website](https://ahoydtu.de)


## Success Stories
- [Getting the data into influxDB and visualize them in a Grafana Dashboard](https://grafana.com/grafana/dashboards/16850-pv-power-ahoy/) (thx @Carl)

## Support, Feedback, Information and Discussion
- [Discord Server (~ 1200 Users)](https://discord.gg/WzhxEY62mB)
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
