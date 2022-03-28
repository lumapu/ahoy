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

    $ cat log2.log | grep 'cmd=2'
    [...]
    2022-03-28T17:36:53.018058Z MSG src=74608145, dst=74608145, cmd=2,   u=235.0V, f=49.98Hz, p=2.5W,  uk1=12851, uk2=0, uk3=14266, uk4=1663, uk5=1666
    2022-03-28T17:38:07.309501Z MSG src=74608145, dst=74608145, cmd=2,   u=234.7V, f=49.99Hz, p=2.3W,  uk1=12851, uk2=0, uk3=14266, uk4=1663, uk5=1666
    2022-03-28T17:38:24.378337Z MSG src=74608145, dst=74608145, cmd=2,   u=234.7V, f=49.98Hz, p=2.2W,  uk1=12851, uk2=0, uk3=14266, uk4=1663, uk5=1666
    2022-03-28T17:38:34.417683Z MSG src=74608145, dst=74608145, cmd=2,   u=234.8V, f=49.98Hz, p=2.2W,  uk1=12851, uk2=0, uk3=14267, uk4=1663, uk5=1667
    [...]

A brief example log is supplied in the `example-logs` folder.



Configuration
-------------

Nothing so far, I'm afraid. You can change the serial number of the inverter
that you are trying to talk to by changing the line that defines the
`inv_ser` variable towards the top of `ahoy.py`.


Todo
----

- Ability to talk to multiple inverters
- MQTT gateway
- understand channel hopping
- configurable polling interval
- commands
- picture of setup!
- ...



References
----------

- [1] https://www.mikrocontroller.net/topic/525778
- [2] https://tutorials-raspberrypi.de/funkkommunikation-zwischen-raspberry-pis-und-arduinos-2-4-ghz/
- [3] https://nrf24.github.io/RF24/index.html
- [4] https://nrf24.github.io/RF24/md_docs_linux_install.html
- [5] https://nrf24.github.io/RF24/md_docs_python_wrapper.html

