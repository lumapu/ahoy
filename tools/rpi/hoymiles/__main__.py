#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Hoymiles micro-inverters main application
"""

import sys
import struct
import traceback
import re

import argparse
import yaml
from yaml.loader import SafeLoader
import logging
from logging.handlers import RotatingFileHandler

import time
from suntimes import SunTimes
from datetime import datetime, timedelta

import hoymiles  # import paket on this place, call once: "hoymiles/__init__.py"

################################################################################
# SIGINT  = Interrupt from keyboard (CTRL + C)
# SIGTERM = Signal Handler from terminating processes
# SIGHUP  = Hangup detected on controlling terminal or death of controlling process
# SIGKILL = Signal Handler SIGKILL and SIGSTOP cannot be caught, blocked, or ignored!!
################################################################################
from signal import signal, Signals, SIGINT, SIGTERM, SIGHUP
from os import environ
def signal_handler(sig_num, frame):
    """ Signal Handler 

    param: signal number [signal-name]
    param: frame
    """
    signame = Signals(sig_num).name
    logging.info(f'Stop by Signal <{signame}> ({sig_num})')
    if environ.get('TERM') is not None:
       print (f'\nStop by Signal <{signame}> ({sig_num}) '
            f'at: {time.strftime("%d.%m.%Y %H:%M:%S")}\n')

    if mqtt_client:
       mqtt_client.disco()

    if influx_client:
       influx_client.disco()

    if volkszaehler_client:
       volkszaehler_client.disco()

    sys.exit(0)

""" activate signal handler """
signal(SIGINT,  signal_handler)
signal(SIGTERM, signal_handler)
signal(SIGHUP,  signal_handler)
# signal(SIGKILL, signal_handler) # not used
################################################################################
################################################################################

class SunsetHandler:
    """ Sunset class
    to recognize the times of sunrise, sunset and to sleep at night time

    :param str inverter: inverter serial
    :param retries: tx retry count if no inverter contact
    :type retries: int
    """
    def __init__(self, sunset_config):
        self.suntimes = None
        if sunset_config and sunset_config.get('disabled', True) == False:
            latitude = sunset_config.get('latitude')
            longitude = sunset_config.get('longitude')
            altitude = sunset_config.get('altitude')
            self.suntimes = SunTimes(longitude=longitude, latitude=latitude, altitude=altitude)
            self.nextSunset = self.suntimes.setutc(datetime.utcnow())
            logging.info (f'Sunset today at: {self.nextSunset} UTC')
            # send info to mqtt, if broker configured
            self.sun_status2mqtt()
        else:
            logging.info('Sunset disabled!')

    def checkWaitForSunrise(self):
        if not self.suntimes:
            return
        # if the sunset already happened for today
        now = datetime.utcnow()
        if self.nextSunset < now:
            # wait until the sun rises again. if it's already after midnight, this will be today
            nextSunrise = self.suntimes.riseutc(now)
            if nextSunrise < now:
                tomorrow = now + timedelta(days=1)
                nextSunrise = self.suntimes.riseutc(tomorrow)
            self.nextSunset = self.suntimes.setutc(nextSunrise)
            time_to_sleep = int((nextSunrise - datetime.utcnow()).total_seconds())
            logging.info (f'Next sunrise is at {nextSunrise} UTC, next sunset is at {self.nextSunset} UTC, sleeping for {time_to_sleep} seconds.')
            if time_to_sleep > 0:
               time.sleep(time_to_sleep)
               logging.info (f'Woke up...')

    def sun_status2mqtt(self):
        """ send sunset information every day to MQTT broker """
        if not mqtt_client or not self.suntimes:
            return

        if self.suntimes:
            local_sunrise = self.suntimes.riselocal(datetime.now()).strftime("%d.%m.%YT%H:%M")
            local_sunset  = self.suntimes.setlocal(datetime.now()).strftime("%d.%m.%YT%H:%M")
            local_zone    = self.suntimes.setlocal(datetime.now()).tzinfo.key

            mqtt_client.info2mqtt(f'{dtu_name}/{dtu_serial}', 
                {'dis_night_comm' : 'True', 
                  'local_sunrise' : local_sunrise, 
                   'local_sunset' : local_sunset,
                     'local_zone' : local_zone})
        else:
            mqtt_client.info2mqtt(f'{dtu_name}/{dtu_serial}', {'dis_night_comm': 'False'})
  

def main_loop(ahoy_config):
    """ Main loop """
    # check 'interval' parameter in config-file
    loop_interval = ahoy_config.get('interval', 15)
    logging.info(f"AHOY-MAIN: loop interval : {loop_interval} sec.")
    if (loop_interval <= 0):
        logging.critical("Parameter 'loop_interval' must grater 0 - please check ahoy.yml.")
        # print console message too
        print("Parameter 'loop_interval' must be >0 - please check ahoy.yml - STOP(0)")
        sys.exit(0)

    # check 'transmit_retries' parameter in config-file
    transmit_retries = ahoy_config.get('transmit_retries', 5)
    if (transmit_retries <= 0):
        logging.critical("Parameter 'transmit_retries' must grater 0 - please check ahoy.yml.")
        # print console message too
        print("Parameter 'transmit_retries' must be >0 - please check ahoy.yml - STOP(0)")
        sys.exit(0)

    # get parameter from config-file
    inverters = [inverter for inverter in ahoy_config.get('inverters', [])
                 if not inverter.get('disabled', False)]

    # check all inverter names and serial numbers in config-file
    for inverter in inverters:
        if not 'name' in inverter:
           inverter['name'] = 'hoymiles'
        if not 'serial' in inverter:
           logging.error("No inverter serial number found in ahoy.yml - exit")
           sys.exit(999)

    # init Sunset-Handler object
    sunset = SunsetHandler(ahoy_config.get('sunset'))

    if not hoymiles.HOYMILES_VERBOSE_LOGGING and not hoymiles.HOYMILES_TRANSACTION_LOGGING:
       logging.info(f"MAIN LOOP starts now without any output")

    try:
        do_init = True
        while True:   # MAIN endless LOOP
            # check sunrise and sunset times and sleep in night time
            sunset.checkWaitForSunrise()
            t_loop_start = time.time()

            for inverter in inverters:
                poll_inverter(inverter, do_init, transmit_retries)
            do_init = False

            # calc time to pause main-loop
            time_to_sleep = loop_interval - (time.time() - t_loop_start)
            if time_to_sleep > 0:
               if hoymiles.HOYMILES_VERBOSE_LOGGING:
                  logging.info(f'MAIN-LOOP: sleep for {time_to_sleep} sec.')
               time.sleep(time_to_sleep)
    except Exception as e:
        logging.fatal('Exception catched: %s' % e)
        logging.fatal(traceback.print_exc())
        raise

def poll_inverter(inverter, do_init, retries):
    """
    Send/Receive command_queue, initiate status poll on inverter

    :param str inverter: inverter serial
    :param retries: tx retry count if no inverter contact
    :type retries: int
    """
    inverter_ser     = inverter.get('serial')
    inverter_name    = inverter.get('name')
    inverter_strings = inverter.get('strings')
    inv_str          = str(inverter_ser)

    # Queue at least status data request
    if do_init:
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.InverterDevInform_Simple)) # 00
      command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.InverterDevInform_All))      # 01
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.GridOnProFilePara))        # 02
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.HardWareConfig))           # 03
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.SimpleCalibrationPara))    # 04
      ##command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.SystemConfigPara))         # 05
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.RealTimeRunData_Reality))  # 0c
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.AlarmData))          # 11
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.AlarmUpdate))        # 12
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.RecordData))         # 13
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.InternalData))       # 14
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.GetLossRate))        # 15
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.GetSelfCheckState))  # 1E
      # command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.InitDataState))      # FF
    command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.RealTimeRunData_Debug))  # 0b

    # Put all queued commands for current inverter on air
    while len(command_queue[inv_str]) > 0:
        if hoymiles.HOYMILES_VERBOSE_LOGGING:
           logging.info(f'Poll inverter name={inverter_name} ser={inverter_ser} command={hoymiles.InfoCommands(command_queue[inv_str][0][0]).name}')
        payload = command_queue[inv_str].pop(0)    ## get first object from command queue

        # Send payload {ttl}-times until we get at least one reponse
        payload_ttl = retries
        response = None
        while payload_ttl > 0:
            payload_ttl = payload_ttl - 1
            com = hoymiles.InverterTransaction(
                    radio=hmradio,
                    txpower=inverter.get('txpower', None),
                    dtu_ser=dtu_serial,
                    inverter_ser=inverter_ser,
                    request=next(hoymiles.compose_esb_packet(
                        payload,
                        seq=b'\x80',
                        src=dtu_serial,
                        dst=inverter_ser
                        )))
            while com.rxtx():
                try:
                    response = com.get_payload()
                    payload_ttl = 0
                except Exception as e_all:
                    if hoymiles.HOYMILES_TRANSACTION_LOGGING:
                        logging.error(f'Error while retrieving data: {e_all}')
                    pass

        # Handle response data, if any
        if response:
            if hoymiles.HOYMILES_TRANSACTION_LOGGING:
                logging.debug(f'Payload: {len(response)} bytes: {hoymiles.hexify_payload(response)}')

            # get a ResponseDecoder object to decode response-payload
            decoder = hoymiles.ResponseDecoder(response,  
                    request=com.request,
                    inverter_ser=inverter_ser,
                    inverter_name=inverter_name,
                    strings=inverter_strings
                    )

            result = decoder.decode()             # call decoder object
            data = result.__dict__()              # convert result into python-dict
            if hoymiles.HOYMILES_TRANSACTION_LOGGING:
               logging.debug(f'Decoded: {data}')

            # check result object for output
            if isinstance(result, hoymiles.decoders.StatusResponse):
                if hoymiles.HOYMILES_VERBOSE_LOGGING:
                   logging.info(f"StatusResponse: payload contains {len(data)} elements "
                                f"(power={data['phases'][0]['power']} W - event_count={data['event_count']})")

                # when 'event_count' is changed, add AlarmData-command to queue
                if data is not None and 'event_count' in data:
                    # if event_message_index[inv_str] < data['event_count']:
                    if event_message_index[inv_str] != data['event_count']:
                       event_message_index[inv_str]  = data['event_count']
                       if hoymiles.HOYMILES_VERBOSE_LOGGING:
                          logging.info(f"event_count changed to {data['event_count']} --> AlarmData requested")
                       # add AlarmData-command to queue 
                       command_queue[inv_str].append(hoymiles.compose_send_time_payload(hoymiles.InfoCommands.AlarmData, alarm_id=event_message_index[inv_str]))

                # sent outputs
                if mqtt_client:
                   mqtt_client.store_status(data)

                if influx_client:
                   influx_client.store_status(data)

                if volkszaehler_client:
                   volkszaehler_client.store_status(data)

            # check decoder object for different data types
            if isinstance(result, hoymiles.decoders.HardwareInfoResponse):
               if hoymiles.HOYMILES_VERBOSE_LOGGING:
                  logging.info(f"Firmware version {data['FW_ver_maj']}.{data['FW_ver_min']}.{data['FW_ver_pat']}, "
                               f"build at {data['FW_build_dd']:>02}/{data['FW_build_mm']:>02}/{data['FW_build_yy']}T"
                               f"{data['FW_build_HH']:>02}:{data['FW_build_MM']:>02}, "
                               f"HW revision {data['FW_HW_ID']}")
               if mqtt_client:
                  mqtt_client.store_status(data)

            if isinstance(result, hoymiles.decoders.EventsResponse):
               if hoymiles.HOYMILES_VERBOSE_LOGGING:
                  logging.info(f"EventsResponse: {data['inv_stat_txt']} ({data['inv_stat_num']})")

            if isinstance(result, hoymiles.decoders.DebugDecodeAny):
               if hoymiles.HOYMILES_VERBOSE_LOGGING:
                  logging.info(f"DebugDecodeAny: payload ({data['len_payload']} bytes): {data['payload']}")

def mqtt_on_message(mqtt_client, userdata, message):
    ''' 
    MQTT(PAHO) callcack method to handle receiving payload 
    ( run in thread: "paho-mqtt-client-" - important for signals and Exceptions !)
    a)  when receiving topic ends with "SENSOR" for privat electricity meter
    b)  when receiving topic ends with "command" for runtime faster debugging

    Handle commands to topic
        hoymiles/{inverter_ser}/command
    frame a payload and put onto command_queue

    Inverters must have mqtt.send_raw_enabled: true configured

    This can be used to inject debug payloads
    The message must be in hexlified format

    Use of variables:
    tttttttt gets expanded to a current int(time)

    Example injects exactly the same as we normally use to poll data:
      mosquitto -h broker -t inverter_topic/command -m 800b00tttttttt0000000500000000

    This allows for even faster hacking during runtime

    :param paho.mqtt.client.Client client: mqtt-client instance
    :param dict userdata: Userdata
    :param dict message: mqtt-client message object
    '''
    # print(f"msg-topic: {message.topic} - QoS: {message.qos}")
    # print(f"payload:  ",str(message.payload.decode("utf-8")), "\n")

    # handle specific payload topic
    if message.topic.endswith("SENSOR"):
       if volkszaehler_client:
          volkszaehler_client.store_status(yaml.safe_load(str(message.payload.decode("utf-8"))))

    if message.topic.endswith("command"):
        p_message = message.payload.decode('utf-8').lower()

        # Expand tttttttt to current time for use in hexlified payload
        expand_time = ''.join(f'{b:02x}' for b in struct.pack('>L', int(time.time())))
        p_message = p_message.replace('tttttttt', expand_time)
        logging.info (f"MQTT-command: {message.topic} - {p_message}")

        if (len(p_message) < 2048 and len(p_message) % 2 == 0 and re.match(r'^[a-f0-9]+$', p_message)):
            payload = bytes.fromhex(p_message)
            if hoymiles.HOYMILES_VERBOSE_LOGGING:
               logging.info (f"MQTT-command for {inv_str}: {payload}")

            # commands must start with \x80
            if payload[0] == 0x80:
                # array "command_queue[inv_str]" will be shared to an other thread --> critical section
                command_queue[inv_str].append(hoymiles.frame_payload(payload[1:]))
            else:
                logging.info (f"MQTT-command: must start with \x80: {payload}")
        else:
            logging.info (f"MQTT-command to long (max length: 2048 bytes) - or contains non hex char")

def init_logging(ahoy_config):
    """ init and prepare logging """
    log_config = ahoy_config.get('logging')
    fn = 'hoymiles.log'
    lvl = logging.ERROR
    max_log_filesize = 1000000
    max_log_files = 1

    if log_config:
        fn = log_config.get('filename', fn)
        level = log_config.get('level', 'ERROR')
        if level == 'DEBUG':
            lvl = logging.DEBUG
        elif level == 'INFO':
            lvl = logging.INFO
        elif level == 'WARNING':
            lvl = logging.WARNING
        elif level == 'ERROR':
            lvl = logging.ERROR
        elif level == 'FATAL':
            lvl = logging.FATAL
        max_log_filesize  = log_config.get('max_log_filesize', max_log_filesize)
        max_log_files = log_config.get('max_log_files', max_log_files)

    # define log switches
    if global_config.log_transactions:
       hoymiles.HOYMILES_TRANSACTION_LOGGING = True
       lvl = logging.DEBUG
    if global_config.verbose:
       hoymiles.HOYMILES_VERBOSE_LOGGING     = True

    # start configured logging
    logging.basicConfig(handlers=[RotatingFileHandler(fn, maxBytes=max_log_filesize, backupCount=max_log_files)], 
        format='%(asctime)s %(levelname)s: %(message)s', 
        datefmt='%Y-%m-%d %H:%M:%S.%s', level=lvl)

    logging.info(f'AHOY-logging started for "{dtu_name}" with level: {logging.getLevelName(logging.root.level)}')

if __name__ == '__main__':
    # read commandline parameter
    parser = argparse.ArgumentParser(description='Ahoy - Hoymiles solar inverter gateway', prog="hoymiles")
    parser.add_argument("-c", "--config-file", nargs="?", required=True,
        help="configuration file")
    parser.add_argument("--log-transactions", action="store_true", default=False,
        help="Enable transaction logging output (loglevel must be DEBUG)")
    parser.add_argument("--verbose", action="store_true", default=False,
        help="Enable detailed debug output (loglevel must be DEBUG)")
    global_config = parser.parse_args()

    # Load config file given in commandline parameter
    try:
        if isinstance(global_config.config_file, str):
            with open(global_config.config_file, 'r') as fh_yaml:
                cfg = yaml.load(fh_yaml, Loader=SafeLoader)
        else:
            with open('ahoy.yml', 'r') as fh_yaml:
                cfg = yaml.load(fh_yaml, Loader=SafeLoader)
    except FileNotFoundError:
        logging.error("Could not load config file. Try --help")
        sys.exit(2)
    except yaml.YAMLError as e_yaml:
        logging.error(f'Failed to load config file {global_config.config_file}: {e_yaml}')
        sys.exit(1)

    # read all parameter from configuration file as 'ahoy_config'
    ahoy_config = dict(cfg.get('ahoy', {}))

    # extract 'DTU' parameter
    dtu_serial  = ahoy_config.get('dtu', {}).get('serial', None)
    dtu_name    = ahoy_config.get('dtu', {}).get('name', 'hoymiles-dtu')

    # init and prepare logging
    init_logging(ahoy_config)

    # Prepare for multiple transceivers (radio modules), makes them configurable
    for radio_config in ahoy_config.get('nrf', [{}]):
        hmradio = hoymiles.HoymilesNRF(**radio_config)

    # create MQTT client object
      # if: mqtt-disabled is "true" - only
      # if: mqtt-disabled is "true" AND inverter-mqtt-send_raw_enabled is "true" 
      # if: mqtt topic is defined - only or with other functions
    mqtt_c_obj  = mqtt_client = None                 # create client-obj-placeholder
    mqtt_config = ahoy_config.get('mqtt', None)      # get mqtt-config, if available

    if mqtt_config and (not mqtt_config.get('disabled', False) or mqtt_topic):
       from .outputs import MqttOutputPlugin

       # MQTT_TOPIC array should contain QOS levels as well as topic names.
       # MQTT_TOPIC = [("Server1/kpi1",0),("Server2/kpi2",0),("Server3/kpi3",0)]
       mqtt_topic  = mqtt_config.get('topic', None)     # get topic, if available
       mqtt_topic_array = []                            # create empty array
       if mqtt_topic:
          mqtt_topic_array.append((mqtt_topic, mqtt_config.get('QoS',0)))

       # create MQTT(PAHO) client object with own callback funtion
       mqtt_c_obj = MqttOutputPlugin(mqtt_config, mqtt_on_message)

       if mqtt_c_obj and not mqtt_config.get('disabled', False):
          mqtt_client = mqtt_c_obj

    # create INFLUX client object
    influx_client = None
    influx_config = ahoy_config.get('influxdb', None)
    if influx_config and not influx_config.get('disabled', False):
        from .outputs import InfluxOutputPlugin
        influx_client = InfluxOutputPlugin(
                influx_config.get('url'),
                influx_config.get('token'),
                org=influx_config.get('org', ''),
                bucket=influx_config.get('bucket', None),
                measurement=influx_config.get('measurement', 'hoymiles'))

    # create VOLKSZAEHLER client object
    volkszaehler_client = None
    volkszaehler_config = ahoy_config.get('volkszaehler', {})
    if volkszaehler_config and not volkszaehler_config.get('disabled', False):
        from .outputs import VolkszaehlerOutputPlugin
        volkszaehler_client = VolkszaehlerOutputPlugin(volkszaehler_config)

    # init important runtime variables
    event_message_index = {}
    command_queue = {}
    mqtt_command_topic_subs = []

    for g_inverter in ahoy_config.get('inverters', []): # loop inverters in ahoy_config
        inv_str = str(g_inverter.get('serial'))         # inverter serial number as index
        command_queue[inv_str] = []                     # create empty command-queue
        event_message_index[inv_str] = 0                # init event-queue with value=0

        # if send_raw_enabled, add topic to subscribe command-queue
        if mqtt_c_obj and g_inverter.get('mqtt', {}).get('send_raw_enabled', False):
           mqtt_topic_array.append(
             (g_inverter.get('mqtt', {}).get('topic', f'hoymiles/{inv_str}') + '/command',
              mqtt_config.get('QoS',0)
             ))

    # start subscribe mqtt broker, if requested 'topic' is available
    if mqtt_c_obj and len(mqtt_topic_array) > 0:
       if hoymiles.HOYMILES_VERBOSE_LOGGING:
          logging.info(f'MQTT: subscribe for topic: {mqtt_topic_array}')
       mqtt_c_obj.client.subscribe(mqtt_topic_array)

    # start main-loop
    main_loop(ahoy_config)

