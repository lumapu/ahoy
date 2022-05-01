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

- RaspberryPi Model 2B (any model should work)
- NRF24L01+ Radio Module connected as described, e.g., in [2]
  (Instructions at [3] should work identically, but [2] has more
  pretty pictures.)
- TMRh20's 'Optimized High Speed nRF24L01+ Driver' [3], installed
  as per the instructions given in [4]
  - Python Library Wrapper, as per [5]



Example Run
-----------

The following command will run the communication tool, which will try to 
contact the inverter every second on channel 40, and listen for replies.

Whenever it sees a reply, it will decoded and logged to the given log file.

    $ sudo python3 ahoy.py | tee -a log2.log



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



Configuration
-------------

Local settings are read from ~/ahoy.conf  
An example is provided as ahoy.conf.example

Todo
----

- Ability to talk to multiple inverters
- MQTT gateway
- understand channel hopping
- configurable polling interval
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

