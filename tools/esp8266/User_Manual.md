# User Manual Ahoy DTU (on ESP8266)
Version #{VERSION}#
## Introduction
See the repository [README.md](README.md)

## Setup
Assuming you have a running ahoy-dtu and you can access the setup page.
In the initial case or after click "erase settings" the fields for the inverter setup are empty.

Set at least the serial number and a name for each inverter, check "reboot after save" and click the "Save" button.

##  MQTT Output
The ahoy dtu will publish on the following topics
`<CHOOSEN_TOPIC_FROM_SETUP>/<INVERTER_NAME_FROM_SETUP>/ch0/#`

| Topic | Example Value | Remarks |
|---|---|---|
|U_AC | 233.300|actual AC Voltage in Volt|
|I_AC | 0.300 | actual AC Current in Ampere|
|P_AC | 71.000| actual AC Power in Watt|
|P_ACr | 21.200| actual AC reactive power in VAr|
|Freq | 49.990|actual AC Frequency in 1/s|
|Pct | 95.800|actual AC Power factor in %|
|Temp | 19.800|Temperature of inverter in Celsius|
|LARM_MES_ID | 9.000|Last Alarm Message Id|
|YieldDay | 51.000|Energy converted to AC per day in Watt hours (measured on DC)|
|YieldTotal | 465.294|Energy converted to AC since reset Watt hours (measured on DC)|
|P_DC | 74.600|actual DC Power in Watt|
|Efficiency | 95.174|actual ration AC Power over DC Power in percent|
|FWVersion | 10012.000| Firmware version eg. 1.00.12|
|FWBuildYear | 2020.000| Firmware build date|
|FWBuildMonthDay | 624.000| Firmware build month and day eg. 24th of june|
|HWPartId | 100.000| Hardware Id|
|PowerLimit | 80.000|actual set point for power limit control AC active power in percent|
|LastAlarmCode | 1.000| Last Alarm Code eg. "inverter start"|

`<CHOOSEN_TOPIC_FROM_SETUP>/<INVERTER_NAME_FROM_SETUP>/ch<CHANNEL_NUMBER>/#`

`<CHANNEL_NUMBER>` is in the range 1 to 4 depending on the inverter type

| Topic | Example Value | Remarks |
|---|---|---|
|U_DC | 38.900| actual DC Voltage in Volt|
|I_DC | 0.640 | actual DC current in Ampere|
|P_DC | 25.000 | actual DC power in Watt|
|YieldDay | 17.000 | Energy converted to AC per day Watt hours per module/channel (measured on DC) |
|YieldTotal | 110.819 | Energy converted to AC since reset Watt hours per module/channel (measured on DC) |
|Irradiation |5.65 | ratio DC Power over set maximum power per module/channel in percent |

## Active Power Limit via Setup Page
If you leave the field "Active Power Limit" empty during the setup and reboot the ahoy-dtu will set a value of 65535 in the setup.
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
Also an absolute active power limit below approx. 30 Watt seems to be not meanful because of the control capabilities and reactive power load.

## Active Power Limit via MQTT
The ahoy-dtu subscribes on the topic `<CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/#` if the mqtt broker is set-up correctly. The default topic is `inverter/devcontrol/#`.

To set the active power limit (controled value is the AC Power of the inverter) you have four options. (Only single phase inverters are actually in focus).

| topic                                                           | payload     | active power limit in                            | Condition      |
| --------------------------------------------------------------- | ----------- | -------------------------------------------- | -------------- |
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11 OR <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/0 | [0..65535] | Watt | not persistent |
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/256                                                         | [0..65535] | Watt | persistent |
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/1                                                           | [2..100]   | %    | not persistent |
| <CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11/257                                                         | [2..100]   | %    | persistent |

👆 `<INVERTER_ID>` is the number of the specific inverter in the setup page.

* First inverter --> `<INVERTER_ID>` = 0
* Second inverter --> `<INVERTER_ID>` = 1
* ...

### Developer Information MQTT Interface
`<CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/<DevControlCmdType>/<DATA2>`

The implementation allows to set any of the available `<DevControlCmdType>` Commands:
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
The MQTT payload will be set on first to bytes and `<DATA2>`, which is taken from the topic path will be set on the second two bytes if the corresponding DevControlCmdType supports 4 byte data.
See here the actual implementation to set the send buffer bytes.
```C
void sendControlPacket(uint64_t invId, uint8_t cmd, uint16_t *data) {
    sendCmdPacket(invId, TX_REQ_DEVCONTROL, ALL_FRAMES, false);
    int cnt = 0;
    // cmd --> 0x0b => Type_ActivePowerContr, 0 on, 1 off, 2 restart, 12 reactive power, 13 power factor
    mTxBuf[10] = cmd;
    mTxBuf[10 + (++cnt)] = 0x00;
    if (cmd >= ActivePowerContr && cmd <= PFSet){
        mTxBuf[10 + (++cnt)] = ((data[0] * 10) >> 8) & 0xff; // power limit || high byte from MQTT payload
        mTxBuf[10 + (++cnt)] = ((data[0] * 10)     ) & 0xff; // power limit || low byte from MQTT payload
        mTxBuf[10 + (++cnt)] = ((data[1]     ) >> 8) & 0xff; // high byte from MQTT topic value <DATA2>
        mTxBuf[10 + (++cnt)] = ((data[1]     )     ) & 0xff; // low byte from MQTT topic value <DATA2>
    }
    // crc control data
    uint16_t crc = Hoymiles::crc16(&mTxBuf[10], cnt+1);
    mTxBuf[10 + (++cnt)] = (crc >> 8) & 0xff;
    mTxBuf[10 + (++cnt)] = (crc     ) & 0xff;
    // crc over all
    cnt +=1;
    mTxBuf[10 + cnt] = Hoymiles::crc8(mTxBuf, 10 + cnt);

    sendPacket(invId, mTxBuf, 10 + (++cnt), true);
}
```

So as example sending any payload on `inverter/devcontrol/0/1` will switch off the inverter.

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
In the same approach as for MQTT any other SubCmd and also MainCmd can be applied and the response payload can be observed in the serial logs. Eg. request the Alarm-Data from the Alarm-Index 5 from inverter 0 will look like this:
```json
{
    "inverter":0,
    "tx_request": 21,
    "cmd": 17,
    "payload": 5,
    "payload2": 0
}
```

## Zero Export Control
* You can use the mqtt topic `<CHOOSEN_TOPIC_FROM_SETUP>/devcontrol/<INVERTER_ID>/11` with a number as payload (eg. 300 -> 300 Watt) to set the power limit to the published number in Watt. (In regular cases the inverter will use the new set point within one intervall period; to verify this see next bullet)
* You can check the inverter set point for the power limit control on the topic `<CHOOSEN_TOPIC_FROM_SETUP>/<INVERTER_NAME_FROM_SETUP>/ch0/PowerLimit` 👆 This value is ALWAYS in percent of the maximum power limit of the inverter. In regular cases this value will be updated within approx. 15 seconds. (depends on request intervall)
* You can monitor the actual AC power by subscribing to the topic `<CHOOSEN_TOPIC_FROM_SETUP>/<INVERTER_NAME_FROM_SETUP>/ch0/P_AC` 👆 This value is ALWAYS in Watt

## Issues and Debuging for active power limit settings

Turn on the serial debugging in the setup. Try to have find out if the behavior is deterministic. That means can you reproduce the behavior. Be patient and wait on inverter reactions at least some minutes and beware that the DC-Power is sufficient.

In case of issues please report:

1. Version of firmware
2. The output of the serial debug esp. the TX messages starting with "0x51" and the RX messages starting with "0xD1" or "0xF1"
3. Which case you have tried: Setup-Page, MQTT, REST API and at what was shown on the "Visualization Page" at the Location "Limit"
4. The setting means payload, relative, absolute, persistent, not persistent (see tables above)

**Developer Information General for Active Power Limit**

⚡The following was verified by field tests and feedback from users

Internally this values will be set for the second two bytes for MainCmd: 0x51 SubCmd: 0x0b --> DevControl set ActivePowerLimit
```C
typedef enum {
    AbsolutNonPersistent    = 0x0000, // 0
    RelativNonPersistent    = 0x0001, // 1
    AbsolutPersistent       = 0x0100,    // 256
    RelativPersistent       = 0x0101     // 257
} PowerLimitControlType;
```

## Firmware Version collection
Gather user inverter information here to understand what differs between some inverters.

| Name       | Inverter Typ | Bootloader V. | FWVersion | FWBuild [YYYY] | FWBuild [MM-DD] | HWPartId  |          |           |
| ---------- | ------------ | ------------- | --------- | -------------- | --------------- | --------- | -------- | --------- |
| DanielR92  | HM-1500      |               | 1.0.16    | 2021           | 10-12           | 100       |          |           |
| isdor      | HM-300       |               | 1.0.14    | 2021           | 12-09           | 102       |          |           |
| aschiffler | HM-1500      |               | 1.0.12    | 2020           | 06-24           | 100       |          |           |
| klahus1    | HM-300       |               | 1.0.10    | 2020           | 07-07           | 102       |          |           |
| roku133    | HM-400       |               | 1.0.10    | 2020           | 07-07           | 102       |          |           |
| eeprom23   | HM-1200      | 0.1.0         | 1.0.18    | 2021           | 12-24           | 269619201 | 18:21:00 | HWRev 256 |
| eeprom23   | HM-1200 2t   | 0.1.0         | 1.0.16    | 2021           | 10-12           | 269619207 | 17:06:00 | HWRev 256 |
| fila612    | HM-700       |               | 1.0.10    | 2021           | 11-01           | 104       |          |           |
| tfhcm      | TSUN-350     |               | 1.0.14    | 2021           | 12-09           | 102       |          |           |
| Groobi     | TSOL-M400    |               | 1.0.14    | 2021           | 12-09           | 102       |          |           |
| setje      | HM-600       |               | 1.0.08    | 2020           | 07-10           | 104       |          |           |
| madmartin  | HM-600       | 0.1.4         | 1.0.10    | 2021           | 11-01           | 104       |          |           |
| lumapu     | HM-1200      | 0.1.0         | 1.0.12    | 2020           | 06-24           |           |          |           |
| chehrlic   | HM-600       |               | 1.0.10    | 2021           | 11-01           | 104       |          |           |
| chehrlic   | TSOL-M800de  |               | 1.0.10    | 2021           | 11-01           | 104       |          |           |
|            |              |               |           |                |                 |           |          |           |
|            |              |               |           |                |                 |           |          |           |

## Developer Information about Command Queue
After reboot or startup the ahoy firmware it will enque three commands in the following sequence:

1. Get active power limit in percent (`SystemConfigPara = 5 // 0x05`)
2. Get firmware version (`InverterDevInform_All = 1 // 0x01`)
3. Get data (`RealTimeRunData_Debug = 11 // 0x0b`)

With the command get data (`RealTimeRunData_Debug = 11 // 0x0b`) the alarm message counter will be updated. In the initial case then aonther command is queued to get the alarm code (`AlarmData = 17 // 0x11`).

This command (`AlarmData = 17 // 0x11`) will enqued in any operation phase if alarm message counter is raised by one or greater compared to the last request with command get data (`RealTimeRunData_Debug = 11 // 0x0b`)

In case all commands are processed (`_commandQueue.empty() == true`) then as a default command the get data (`RealTimeRunData_Debug = 11 // 0x0b`) will be enqueued.

In case a Device Control command (Power Limit, Off, On) is requested via MQTT or REST API this request will be send before any other enqueued command.
In case of a accepted change in power limit the command  get active power limit in percent (`SystemConfigPara = 5 // 0x05`) will be enqueued. The acceptance is checked by the reponse packets on the devive control commands (tx id 0x51 --> rx id 0xD1) if in byte 12 the requested sub-command (eg. power limit) is present.
