Communicating with Hoymiles Micro-Inverters using Python on RaspberryPi
=======================================================================

The tools in this folder (and subfolders) provide the ability to
communicate with Hoymiles micro-inverters.

They require the hardware setup described below.

The tools are still quite rudimentary, as the communication 
behaviour is not yet fully understood.

This is part of an ongoing group effort, and the knowledge gained so
far is the result of a crowd effort that started at [1].

Thanks go to all who contributed, and are continuing to contribute,
by providing their time, equipment, and ingenuity!


Required Hardware Setup
-----------------------

`ahoy.py` has been successfully tested with the following setup

- RaspberryPi Model 2B, 4B (any model should work)
- NRF24L01+ Radio Module connected as described, e.g., in [2]
  (Instructions at [3] should work identically, but [2] has more
  pretty pictures.)
- or the [PaHoy board](https://github.com/DM6JM/PaHoy/)
- TMRh20's 'Optimized High Speed nRF24L01+ Driver' [3], installed 
  as per the instructions given in [4]
  - Python Library Wrapper, as per [5]
- or the easy way, using [pyRF24](https://github.com/nRF24/pyRF24)[6]

How to talk to the nRF24L01+ in Python?
---------------------------------------
Either you make use of the way proposed in the following, using the NRF24 Python Wrapper and the 'Optimized High Speed nRF24L01+ Driver' OR you just use pip and let it install pyRF24.

- If you go with pyRF24, all that needs to be done is installing pyRF24 as described in [6]. Please be aware that not all examples provided in this repo are prepared to use pyRF24. It might be nescessary to adjust the imports from RF24 to pyRF24 to get them running. Once you installed pyRF24, go on at 'Required python modules'
- If you go with the RF24 wrapper, do the following steps

Building the NRF24 Python Wrapper
---------------------------------

You have to install the NRF24 Python Library, as a Dependency for the Raspberry Pi Version of Ahoy.

To do that correctly, I have contacted the developer of NRF24 via github
[Python 3 Wrapper not installing properly #845](https://github.com/nRF24/RF24/issues/845) 
as I could not get the Python Wrapper for NRF24 to be built.

- Install Raspberry Pi OS lite x86 with raspberry pi imager
- Connect nrf24 module to raspberry pi (as described in github)
- Login with user pi
- Execute `sudo apt update && sudo apt -y upgrade`
- Execute `sudo raspi-config` and
  - Select "Expand filesystem" in "Advanced Options"
  - Activate "SPI" in "Interface Options"
  - "Finish" to exit `raspi-config` Tool, reboot YES!
- Login as pi user again

```code
sudo apt install cmake git python3-dev libboost-python-dev python3-pip python3-rpi.gpio

sudo ln -s $(ls /usr/lib/$(ls /usr/lib/gcc | \
     tail -1)/libboost_python3*.so | \
     tail -1) /usr/lib/$(ls /usr/lib/gcc | \
     tail -1)/libboost_python3.so

git clone https://github.com/nRF24/RF24.git
cd RF24

export RF24_DRIVER=SPIDEV
rm Makefile.inc #just to make sure there is no old stuff
mkdir build && cd build
cmake ..
make
sudo make install

cd ../pyRF24
rm -r ./build/ ./dist/ ./RF24.egg-info/ ./__pycache__/ #just to make sure there is no old stuff
python3 -m pip install --upgrade pip
python3 -m pip install .
python3 -m pip list #watch for RF24 module - if its there its installed

cd ..
cd examples_linux/
python3 getting_started.py # to test and see whether RF24 class can be loaded as module in python correctly
```

If there are no error messages on the last step, then the NRF24 Wrapper has been installed successfully.


Building RF24 Wrapper for Debian 11 (bullseye) 64 bit operating system
----------------------------------------------------------------------
The description above does not work on Debian 11 (bullseye) 64 bit operating system.
Please check first, if you have Debian 11 (bullseye) 64 bit operating system installed:
 - `uname -a` search for aarch64
 - `lsb_release -d`
 - `cat /etc/debian_version`

There are 2 possible solutions to install the RF24 wrapper:

**__1. Solution:__**
```code
sudo apt install cmake git python3-dev libboost-python-dev python3-pip python3-rpi.gpio

sudo ln -s $(ls /usr/lib/$(ls /usr/lib/gcc | \
     head -1)/libboost_python3*.so | \
     tail -1) /usr/lib/$(ls /usr/lib/gcc | \
     head -1)/libboost_python3.so

git clone https://github.com/nRF24/RF24.git
cd RF24

rm -rf build Makefile.inc 
./configure --driver=SPIDEV
```
> _edit `Makefile.inc` with your prefered editor e.g. nano or vi_
> 
> old:
>```code
> CPUFLAGS=-marm -march=armv6zk -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard
> CFLAGS=-marm -march=armv6zk -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -Ofast -Wall -pthread
>```
> new:
>```code
> CPUFLAGS=
> CFLAGS=-Ofast -Wall -pthread
>```
_continue now_ 
```code
make
sudo make install

cd pyRF24
rm -r ./build/ ./dist/ ./RF24.egg-info/ ./__pycache__/ #just to make sure there is no old stuff
python3 -m pip install --upgrade pip
python3 -m pip install .
python3 -m pip list #watch for RF24 module - if its there its installed
```


**__2. Solution:__**
```code
sudo apt install git python3-dev libboost-python-dev python3-pip python3-rpi.gpio

git clone --recurse-submodules https://github.com/nRF24/pyRF24.git
cd pyRF24
python3 -m pip install . -v     # this step takes about 5 minutes on my RPI-4 !
```

If you have problems with your radio module from ahoi, e.g.: cannot interpret received data, 
please try to reduce the speed of your radio module!
Add the following parameter to your ahoy.yml configuration file in "nrf" section:
`spispeed: 600000` (0.6 MHz)



Required python modules
-----------------------

Some modules are not installed by default on a RaspberryPi, therefore add them manually:

```code
pip install crcmod pyyaml paho-mqtt SunTimes
```

Configuration
-------------

Local settings are read from ahoy.yml  
An example is provided as ahoy.yml.example


Example Run
-----------

The following command will run the communication tool, which will try to 
contact the inverter every second on channel 40, and listen for replies.

Whenever it sees a reply, it will decoded and logged to the given log file.

    $ sudo python3 -um hoymiles --log-transactions --verbose --config /home/dtu/ahoy.yml | tee -a log2.log

Python parameters
- `-u` enables python's unbuffered mode
- `-m hoymiles` tells python to load module 'hoymiles' as main app


The application describes itself
```code
python3 -m hoymiles --help
usage: hoymiles [-h] -c [CONFIG_FILE] [--log-transactions] [--verbose]

Ahoy - Hoymiles solar inverter gateway

optional arguments:
  -h, --help            show this help message and exit
  -c [CONFIG_FILE], --config-file [CONFIG_FILE]
                        configuration file
  --log-transactions    Enable transaction logging output
  --verbose             Enable debug output
```


Inject payloads via MQTT
------------------------

To enable mqtt payload injection, this must be configured per inverter
```yaml
...
  inverters:
...
    - serial: 1147112345
      mqtt:
        send_raw_enabled: true
...
```

This can be used to inject debug payloads
The message must be in hexlified format

Use of variables:
  * `tttttttt` expands to current time like we know from our `80 0b` command

Example injects exactly the same as we normally use to poll data

    $ mosquitto_pub -h broker -t inverter_topic/command -m 800b00tttttttt0000000500000000

This allows for even faster hacking during runtime

Running it as a service
-----------------------
If you want to run directly from the start, you might want to install it as a service.
Depending on if you want to run it once a user is logged in or as soon as the system is booted, two service examples are included.
ahoy.service allows you to start it as a user service upon login.
ahoy_system.service allows you to start it as a system service already before login without user interaction.

Analysing the Logs
------------------

Use basic command line tools to get an idea what you recorded. For example:

    $ cat log2.log
    [...]
    2022-05-02 16:41:16.044179 Transmit | 15 72 22 01 43 78 56 34 12 80 0b 00 62 3c 8e cf 00 00 00 05 00 00 00 00 35 a3 08
    2022-05-02 17:01:41.844361 Received 27 bytes on channel 3: 95 72 22 01 43 72 22 01 43 01 00 01 01 44 00 4e 00 fe 01 46 00 4f 01 02 00 00 6b
    2022-05-02 17:01:41.886796 Received 27 bytes on channel 75: 95 72 22 01 43 72 22 01 43 02 8f 82 00 00 86 7a 05 fe 06 0b 08 fc 13 8a 01 e9 15
    2022-05-02 17:01:41.934667 Received 23 bytes on channel 75: 95 72 22 01 43 72 22 01 43 83 00 00 00 15 03 e8 00 df 03 83 d5 f3 91
    2022-05-02 17:01:41.934667 Decoded: 44 string1= 32.4VDC 0.78A 25.4W 36738Wh 1534Wh/day string2= 32.6VDC 0.79A 25.8W 34426Wh 1547Wh/day phase1= 230.0VAC 2.1A 48.9W inverter=114171230143 50.02Hz 22.3°C
    [...]

A brief example log is supplied in the `example-logs` folder.




Todo
----

- Ability to talk to multiple inverters
- MQTT gateway
- understand channel hopping
- ~~configurable polling interval~~ done: interval ist configurable in ahoy.yml
- commands
- picture of setup!
- python module
- ...



References
----------

- [1] https://www.mikrocontroller.net/topic/525778
- [2] https://tutorials-raspberrypi.de/funkkommunikation-zwischen-raspberry-pis-und-arduinos-2-4-ghz/
- [3] https://nrf24.github.io/RF24/index.html
- [4] https://nrf24.github.io/RF24/md_docs_linux_install.html
- [5] https://nrf24.github.io/RF24/md_docs_python_wrapper.html
- [6] https://github.com/nRF24/pyRF24
