[![CC BY-NC-SA 4.0][cc-by-nc-sa-shield]][cc-by-nc-sa] [![Ahoy Dev Build][dev-action-badge]][dev-action-link]

This work is licensed under a
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: https://creativecommons.org/licenses/by-nc-sa/4.0/deed.de
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg

[dev-action-badge]: https://github.com/lumapu/ahoy/actions/workflows/compile_development.yml/badge.svg
[dev-action-link]: https://github.com/lumapu/ahoy/actions/workflows/compile_development.yml


# 🖐 Ahoy!
![Logo](https://github.com/grindylow/ahoy/blob/main/doc/logo1_small.png?raw=true)

This repository offers hardware and software solutions for communicating with Hoymiles inverters via radio. With our system, you can easily obtain real-time values such as power, current, and daily energy. Additionally, you can set parameters like the power limit of your inverter to achieve zero export. You can access these functionalities through our user-friendly web interface, MQTT, or JSON. Whether you're monitoring your solar panel system's performance or fine-tuning its settings, our solutions make it easy to achieve your goals.

Table of approaches:

| Board  | MI | HM | HMS/HMT | comment | HowTo start |
| ------ | -- | -- | ------- | ------- | ---------- |
| [ESP8266/ESP32, C++](Getting_Started.md) | ✔️ | ✔️ | coming soon✨ |  👈 the most effort is spent here | [create your own DTU](https://ahoydtu.de/getting_started/) |
| [Arduino Nano, C++](tools/nano/NRF24_SendRcv/) | ❌ | ✔️ | ❌ | |
| [Raspberry Pi, Python](tools/rpi/) | ❌ | ✔️ | ❌ | |
| [Others, C/C++](tools/nano/NRF24_SendRcv/) | ❌ | ✔️ | ❌ |  |

## Getting Started
[Guide how to start with a ESP module](Getting_Started.md)

[ESP Webinstaller (Edge / Chrome Browser only)](https://ahoydtu.de/web_install)

## Our Website
[https://ahoydtu.de](https://ahoydtu.de)

## Success Stories
- [Getting the data into influxDB and visualize them in a Grafana Dashboard](https://grafana.com/grafana/dashboards/16850-pv-power-ahoy/) (thx @Carl)

## Support, Feedback, Information and Discussion
- [Discord Server (~ 3.800 Users)](https://discord.gg/WzhxEY62mB)
- [The root of development](https://www.mikrocontroller.net/topic/525778)

### Development
If you run into any issues, please feel free to use the issue tracker here on Github. When describing your issue, please be as detailed and precise as possible, and take a moment to consider whether the issue is related to our software. This will help us to provide more effective solutions to your problem.

**Contributors are always welcome!**

### Related Projects
- [OpenDTU](https://github.com/tbnobody/OpenDTU)
  <- Our sister project ✨ for Hoymiles HM-300, HM-600, HM-1200 (for ESP32 only!)
- [DTU Simulator](https://github.com/Ziyatoe/DTUsimMI1x00-Hoymiles) 
  <- Go here ✨ for Hoymiles MI-300, MI-600, MI-1200 Software (single inverter only)
