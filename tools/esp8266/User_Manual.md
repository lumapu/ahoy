# User Manual Ahoy DTU (on ESP8266)
16.08.2022
## Introduction
See the repository [here](https://github.com/grindylow/ahoy/blob/main/tools/esp8266/README.md)

## Setup
Assuming you have a running ahoy-dtu and you can access the setup page.
In the initial case or after click "erase settings" the fields for the inverter setup are empty.

Set at least the serial number and a name for each inverter, check the "reboot after save" and click the "Save" button.

## Active Power Limit via Setup Page
If you leave the field "Active Power Limit" empty during the setup and reboot the ahoy-dtu a value of 65535 will be filled in.
That is the value you have to fill in case you want to operate the inverter without a active power limit.
If the value is 65535 or -1 after another reboot the value will be set automatically to "100" and in the drop-down menu "relative in percent persistent" will be set. Of course you can do this also by your self.

You can change the setting in the following manner.
Decide if you want to set
- an absolute value in Watt
- an relative value in percent based on the maximum Power capabilities of the inverter
and if this settings shall be
- persistent
- not persistent
after a power cycle of the inverter (P_DC=0 and P_AC=0 for at least 10 seconds)

The user has to ensure correct settings. Remember that for the inverters of 3rd generation the relative active power limit is in the range of 2% up to 100%.
Also an absolute active power limit below approx. 30Watt is not correct because of the control capabilities and reactive power load.

## Active Power Limit via MQTT
The ahoy-dtu subscribes on the topic <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/# if the mqtt broker is set-up correctly. The default topic is inverter/devcontrol/#.

To set the absolut active power limit you have four options.


| topic                                                           | payload     | active power limit in                            | Condition      |
| --------------------------------------------------------------- | ----------- | -------------------------------------------- | -------------- |
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11 OR <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/0 | [0..65535]  | Watt | not persistent 
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/256                                                         | [0...65535] | Watt       | persistent
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/1                                                           | [2...100]   | %  | not persistent
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/257                                                         | [2...100]   | % |  persistent

### Developer Information MQTT Interface
<CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/<DevControlCmdType>/<DATA2>

The implementation allows to set any of the available <DevCntrlType> Commands:
```C
    typedef enum {
    TurnOn = 0,                   // 0x00
    TurnOff = 1,                  // 0x01
    Restart = 2,                  // 0x02
    Lock = 3,                     // 0x03
    Unlock = 4,                   // 0x04
    ActivePowerContr = 11,        // 0x0b
    ReactivePowerContr = 12,      // 0x0c
    PFSet = 13,                   // 0x0d
    CleanState_LockAndAlarm = 20, // 0x14
    SelfInspection = 40,          // 0x28, self-inspection of grid-connected protection files
    Init = 0xff
} DevControlCmdType;
```
The MQTT payload will be set on first to bytes and DATA2 will be set on the second two bytes if the corresponding DevControlCmdType supports 4 byte data.

So as example sending any payload on inverter/devcontrol/0/1 will switch off the inverter.

## Active Power Limit via REST API
It is also implemented to set the power limit via REST API call. Therefore send a POST request to the endpoint /api.
The response will always be a json with {success:true}
The payload shall be a json formated string in the following manner
```json
{
    "inverter":<INVERTER_ID>,
    "tx_request": <TX_REQUEST_BYTE>,
    "cmd": <SUB_CMD_BYTE>,
    "payload": <PAYLOAD_INTEGER_TWO_BYTES>,
    "payload2": <PAYLOAD_INTEGER_TWO_BYTES>
}
```
With the following value ranges


| Value                       | range       | note                            |
| --------------------------- | ----------- | ------------------------------- |
| <TX_REQUEST_BYTE>           | 81 or 21    | integer uint8, (0x15 or 0x51)   |
| <SUB_CMD_BYTE>              | [0...255]   | integer uint8, subcmds eg. 0x0b |
| <PAYLOAD_INTEGER_TWO_BYTES> | [0...65535] | uint16                          |
| <INVERTER_ID>               | [0...3]     | integer uint8                   |

Example to set the active power limit non persistent to 10%
```json
{
    "inverter":0,
    "tx_request": 81,
    "cmd": 11,
    "payload": 10,
    "payload2": 1
}
```
Example to set the active power limit persistent to 600Watt
```json
{
    "inverter":0,
    "tx_request": 81,
    "cmd": 11,
    "payload": 600,
    "payload2": 256
}
```

### Developer Information REST API
In the same approach as for MQTT any other SubCmd can be applied and the response payload can be observed in the serial logs. Eg. request the Alarm Data.


## Issues and Debuging for active power limit settings
Turn on the serial debugging in the setup. Try to have find out if the behavior is deterministic. That means can you reproduce the behavior. Be patient and wait on inverter reactions at least some minutes and beware that the DC-Power is sufficient.
In case of issues please report:
1. Version of firmware
2. The output of the serial debug esp. the TX messages starting with "0x51" and the RX messages starting with "0xD1" or "0xF1"
3. Which case you have tried: Setup-Page, MQTT, REST API and at what was shown on the "Visualization Page" at the Location "Limit"
4. The setting means payload, relative, absolute, persistent, not persistent (see tables above)


**Developer Information General for Active Power Limit**
⚡To be verified by field tests and feedback
Internally this values will be set for the second two bytes for MainCmd: 0x51 SubCmd: 0x0b --> DevControl set ActivePowerLimit
```C
typedef enum { // ToDo: to be verified by field tests
    AbsolutNonPersistent = 0x0000, // 0
    RelativNonPersistent = 0x0001, // 1
    AbsolutPersistent = 0x0100,    // 256
    RelativPersistent = 0x0101  // 257
} PowerLimitControlType;
```
