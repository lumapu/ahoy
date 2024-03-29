# Ahoy configuration

## Prerequists
You have build your own hardware (or purchased one). The firmware is already loaded on the ESP and the WebUI is accessible from your browser.

## Start
But how do I get my data from the inverter?

The following steps are required:
1. Set the pinning to communicate with the radio module.
2. Check if Ahoy has a current time
3. Configure the inverter data (e.g. serialnumber)

### 1.) Set the pinning
Once you are in the web interface, you will find the "System Config" sub-item in the Setup area.

This is where you tell the ESP how you connected the radio module.
Note the schematics you saw earlier. - If you haven't noticed them yet, here's another table of connections.


#### OpenDTU Fusion (ESP32-S3)
| NRF24 Pin | ESP Pin|
|---------| --------|
| CS (4) | GPIO37
| CE (3)| GPIO38
| IRQ (8) | GPIO47
| SCLK (5)| GPIO36
| MOSI (6)| GPIO35
| MISO (7)| GPIO48

| CMT2300A  | Pin |
|---------| --------|
| CMT| Enabled |
| SCLK| GPIO6
| SDIO| GPIO5
| CSB| GPIO4
| FCSB| GPIO21
| GPIO3| GPIO8

### 2.) Set current time (standard: skip this step)
Ahoy needs a current date and time to talk to the inverter.
It works without, but it is recommended to include a time. This allows you to analyze information from the inverter in more detail.
Normally, a date/time should be automatically retrieved from the NTP server. However, it may happen that the firewall of some routers does not allow this.
In the section "Settings -> NTP Server" you can also get the time from your own computer. Or set up your own NTP server.

### 3.) Set inverter data

#### add new inverter
Now it's time to place the inverter. This is necessary because it is not the inverter that speaks first, but the DTU (Ahoy).

Each inverter has its own S.Nr. This also serves as an identity for communication between the DTU and the inverter.

The S.Nr is a 12-digit number. Check [here (german)](https://github.com/lumapu/ahoy/wiki/Hardware#wie-ist-die-serien-nummer-der-inverter-aufgebaut) for more information.

#### set pv-modules (not necessary)
Click on "Add Inverter" and enter the S.No. and a name. Please keep the name short!
![grafik](https://github.com/lumapu/ahoy/doc/screenshots/settings.png)

![grafik](https://github.com/lumapu/ahoy/doc/screenshots/inverterSettings.png)

In the upper tab "Inputs" you can enter the data of the solar modules. These are only used directly in Ahoy for calculation and have no influence on the inverter.

#### set radio parameter (not necessary, only for EU)
In the next tab "Radio" you can adjust the power and other parameters if necessary. However, these should be left as default (EU only).

#### advanced options (not necessary to be changed)
In the "Advanced" section, you can customize more settings.

Save and reboot.

## ✅ Done - Now check the live site
