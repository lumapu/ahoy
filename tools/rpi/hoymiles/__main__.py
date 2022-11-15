#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Hoymiles micro-inverters main application
"""

import sys
import struct
from enum import IntEnum
import re
import time
import traceback
from datetime import datetime
from datetime import timedelta
from suntimes import SunTimes
import argparse
import yaml
from yaml.loader import SafeLoader
import paho.mqtt.client
import hoymiles
import logging

class InfoCommands(IntEnum):
    InverterDevInform_Simple = 0  # 0x00
    InverterDevInform_All = 1     # 0x01
    GridOnProFilePara = 2         # 0x02
    HardWareConfig = 3            # 0x03
    SimpleCalibrationPara = 4     # 0x04
    SystemConfigPara = 5          # 0x05
    RealTimeRunData_Debug = 11    # 0x0b
    RealTimeRunData_Reality = 12  # 0x0c
    RealTimeRunData_A_Phase = 13  # 0x0d
    RealTimeRunData_B_Phase = 14  # 0x0e
    RealTimeRunData_C_Phase = 15  # 0x0f
    AlarmData = 17                # 0x11, Alarm data - all unsent alarms
    AlarmUpdate = 18              # 0x12, Alarm data - all pending alarms
    RecordData = 19               # 0x13
    InternalData = 20             # 0x14
    GetLossRate = 21              # 0x15
    GetSelfCheckState = 30        # 0x1e
    InitDataState = 0xff

class SunsetHandler:
    def __init__(self, sunset_config):
        self.suntimes = None
        if sunset_config and sunset_config.get('disabled', True) == False:
            latitude = sunset_config.get('latitude')
            longitude = sunset_config.get('longitude')
            altitude = sunset_config.get('altitude')
            self.suntimes = SunTimes(longitude=longitude, latitude=latitude, altitude=altitude)
            self.nextSunset = self.suntimes.setutc(datetime.utcnow())
            logging.info (f'Todays sunset is at {self.nextSunset} UTC')

    def checkWaitForSunrise(self):
        if not self.suntimes:
            return
        # if the sunset already happened for today
        now = datetime.utcnow()
        if self.nextSunset < now:
            # wait until the sun rises tomorrow
            tomorrow = now + timedelta(days=1)
            nextSunrise = self.suntimes.riseutc(tomorrow)
            self.nextSunset = self.suntimes.setutc(tomorrow)
            time_to_sleep = int((nextSunrise - datetime.now()).total_seconds())
            logging.info (f'Waiting for sunrise at {nextSunrise} UTC ({time_to_sleep} seconds)')
            if time_to_sleep > 0:
                time.sleep(time_to_sleep)
                logging.info (f'Woke up... next sunset is at {self.nextSunset} UTC')

def main_loop(ahoy_config):
    """Main loop"""
    inverters = [
            inverter for inverter in ahoy_config.get('inverters', [])
            if not inverter.get('disabled', False)]

    sunset = SunsetHandler(ahoy_config.get('sunset'))
    dtu_ser = ahoy_config.get('dtu', {}).get('serial')

    loop_interval = ahoy_config.get('interval', 1)
    try:
        do_init = True
        while True:
            sunset.checkWaitForSunrise()

            t_loop_start = time.time()

            for inverter in inverters:
                if hoymiles.HOYMILES_DEBUG_LOGGING:
                    logging.debug(f'Poll inverter {inverter["serial"]}')
                poll_inverter(inverter, dtu_ser, do_init, 3)
            do_init = False

            if loop_interval > 0:
                time_to_sleep = loop_interval - (time.time() - t_loop_start)
                if time_to_sleep > 0:
                    time.sleep(time_to_sleep)

    except KeyboardInterrupt:
        sys.exit()
    except Exception as e:
        logging.fatal('Exception catched: %s' % e)
        logging.fatal(traceback.print_exc())
        raise


def poll_inverter(inverter, dtu_ser, do_init, retries):
    """
    Send/Receive command_queue, initiate status poll on inverter

    :param str inverter: inverter serial
    :param retries: tx retry count if no inverter contact
    :type retries: int
    """
    inverter_ser = inverter.get('serial')

    # Queue at least status data request
    inv_str = str(inverter_ser)
    if do_init:
      command_queue[inv_str].append(hoymiles.compose_send_time_payload(InfoCommands.InverterDevInform_All))
#      command_queue[inv_str].append(hoymiles.compose_send_time_payload(InfoCommands.SystemConfigPara))
    command_queue[inv_str].append(hoymiles.compose_send_time_payload(InfoCommands.RealTimeRunData_Debug))

    # Put all queued commands for current inverter on air
    while len(command_queue[inv_str]) > 0:
        payload = command_queue[inv_str].pop(0)

        # Send payload {ttl}-times until we get at least one reponse
        payload_ttl = retries
        while payload_ttl > 0:
            payload_ttl = payload_ttl - 1
            com = hoymiles.InverterTransaction(
                    radio=hmradio,
                    txpower=inverter.get('txpower', None),
                    dtu_ser=dtu_ser,
                    inverter_ser=inverter_ser,
                    request=next(hoymiles.compose_esb_packet(
                        payload,
                        seq=b'\x80',
                        src=dtu_ser,
                        dst=inverter_ser
                        )))
            response = None
            while com.rxtx():
                try:
                    response = com.get_payload()
                    payload_ttl = 0
                except Exception as e_all:
                    logging.error(f'Error while retrieving data: {e_all}')
                    pass

        # Handle the response data if any
        if response:
            c_datetime = datetime.now()
            if hoymiles.HOYMILES_DEBUG_LOGGING:
                logging.debug(f'{c_datetime} Payload: ' + hoymiles.hexify_payload(response))
            decoder = hoymiles.ResponseDecoder(response,
                    request=com.request,
                    inverter_ser=inverter_ser
                    )
            result = decoder.decode()
            if isinstance(result, hoymiles.decoders.StatusResponse):
                data = result.__dict__()

                if hoymiles.HOYMILES_DEBUG_LOGGING:
                    dbg = f'{c_datetime} Decoded: temp={data["temperature"]}, total={data["energy_total"]/1000:.3f}'
                    if data['powerfactor'] is not None:
                        dbg += f', pf={data["powerfactor"]}'
                    phase_id = 0
                    for phase in data['phases']:
                        dbg += f' phase{phase_id}=voltage:{phase["voltage"]}, current:{phase["current"]}, power:{phase["power"]}, frequency:{data["frequency"]}'
                        phase_id = phase_id + 1
                    string_id = 0
                    for string in data['strings']:
                        dbg += f' string{string_id}=voltage:{string["voltage"]}, current:{string["current"]}, power:{string["power"]}, total:{string["energy_total"]/1000}, daily:{string["energy_daily"]}'
                        string_id = string_id + 1
                    logging.debug(dbg)

                if 'event_count' in data:
                    if event_message_index[inv_str] < data['event_count']:
                        event_message_index[inv_str] = data['event_count']
                        command_queue[inv_str].append(hoymiles.compose_send_time_payload(InfoCommands.AlarmData, alarm_id=event_message_index[inv_str]))

                if mqtt_client:
                    mqtt_send_status(mqtt_client, inverter_ser, data,
                            topic=inverter.get('mqtt', {}).get('topic', None))
                if influx_client:
                    influx_client.store_status(result)

                if volkszaehler_client:
                    volkszaehler_client.store_status(result)

def mqtt_send_status(broker, inverter_ser, data, topic=None):
    """
    Publish StatusResponse object

    :param paho.mqtt.client.Client broker: mqtt-client instance
    :param str inverter_ser: inverter serial
    :param hoymiles.StatusResponse data: decoded inverter StatusResponse
    :param topic: custom mqtt topic prefix (default: hoymiles/{inverter_ser})
    :type topic: str
    """

    if not topic:
        topic = f'hoymiles/{inverter_ser}'

    # AC Data
    phase_id = 0
    for phase in data['phases']:
        broker.publish(f'{topic}/emeter/{phase_id}/power', phase['power'])
        broker.publish(f'{topic}/emeter/{phase_id}/voltage', phase['voltage'])
        broker.publish(f'{topic}/emeter/{phase_id}/current', phase['current'])
        phase_id = phase_id + 1

    # DC Data
    string_id = 0
    for string in data['strings']:
        broker.publish(f'{topic}/emeter-dc/{string_id}/total', string['energy_total']/1000)
        broker.publish(f'{topic}/emeter-dc/{string_id}/power', string['power'])
        broker.publish(f'{topic}/emeter-dc/{string_id}/voltage', string['voltage'])
        broker.publish(f'{topic}/emeter-dc/{string_id}/current', string['current'])
        string_id = string_id + 1
    # Global
    if data['powerfactor'] is not None:
        broker.publish(f'{topic}/pf', data['powerfactor'])
    broker.publish(f'{topic}/frequency', data['frequency'])
    broker.publish(f'{topic}/temperature', data['temperature'])
    if data['energy_total'] is not None:
        broker.publish(f'{topic}/total', data['energy_total']/1000)

def mqtt_on_command(client, userdata, message):
    """
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
    """
    try:
        inverter_ser = next(
                item[0] for item in mqtt_command_topic_subs if item[1] == message.topic)
    except StopIteration:
        logging.warning('Unexpedtedly received mqtt message for {message.topic}')

    if inverter_ser:
        p_message = message.payload.decode('utf-8').lower()

        # Expand tttttttt to current time for use in hexlified payload
        expand_time = ''.join(f'{b:02x}' for b in struct.pack('>L', int(time.time())))
        p_message = p_message.replace('tttttttt', expand_time)

        if (len(p_message) < 2048 \
            and len(p_message) % 2 == 0 \
            and re.match(r'^[a-f0-9]+$', p_message)):
            payload = bytes.fromhex(p_message)
            # commands must start with \x80
            if payload[0] == 0x80:
                command_queue[str(inverter_ser)].append(
                    hoymiles.frame_payload(payload[1:]))

def init_logging(ahoy_config):
    log_config = ahoy_config.get('logging')
    fn = 'hoymiles.log'
    lvl = logging.ERROR
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
    logging.basicConfig(filename=fn, format='%(asctime)s %(levelname)s: %(message)s', datefmt='%Y-%m-%d %H:%M:%S', level=lvl)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Ahoy - Hoymiles solar inverter gateway', prog="hoymiles")
    parser.add_argument("-c", "--config-file", nargs="?", required=True,
        help="configuration file")
    parser.add_argument("--log-transactions", action="store_true", default=False,
        help="Enable transaction logging output (loglevel must be DEBUG)")
    parser.add_argument("--verbose", action="store_true", default=False,
        help="Enable detailed debug output (loglevel must be DEBUG)")
    global_config = parser.parse_args()

    # Load ahoy.yml config file
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
        logging.error('Failed to load config file {global_config.config_file}: {e_yaml}')
        sys.exit(1)

    ahoy_config = dict(cfg.get('ahoy', {}))
    init_logging(ahoy_config)

    # Prepare for multiple transceivers, makes them configurable (currently
    # only one supported)
    for radio_config in ahoy_config.get('nrf', [{}]):
        hmradio = hoymiles.HoymilesNRF(**radio_config)

    mqtt_client = None

    event_message_index = {}
    command_queue = {}
    mqtt_command_topic_subs = []

    if global_config.log_transactions:
        hoymiles.HOYMILES_TRANSACTION_LOGGING=True
    if global_config.verbose:
        hoymiles.HOYMILES_DEBUG_LOGGING=True

    mqtt_config = ahoy_config.get('mqtt', [])
    if not mqtt_config.get('disabled', False):
        mqtt_client = paho.mqtt.client.Client()
        
        if mqtt_config.get('useTLS',False):
            mqtt_client.tls_set()
            mqtt_client.tls_insecure_set(mqtt_config.get('insecureTLS',False))

        mqtt_client.username_pw_set(mqtt_config.get('user', None), mqtt_config.get('password', None))
        mqtt_client.connect(mqtt_config.get('host', '127.0.0.1'), mqtt_config.get('port', 1883))
        mqtt_client.loop_start()
        mqtt_client.on_message = mqtt_on_command

    influx_client = None
    influx_config = ahoy_config.get('influxdb', {})
    if influx_config and not influx_config.get('disabled', False):
        from .outputs import InfluxOutputPlugin
        influx_client = InfluxOutputPlugin(
                influx_config.get('url'),
                influx_config.get('token'),
                org=influx_config.get('org', ''),
                bucket=influx_config.get('bucket', None),
                measurement=influx_config.get('measurement', 'hoymiles'))

    volkszaehler_client = None
    volkszaehler_config = ahoy_config.get('volkszaehler', {})
    if volkszaehler_config and not volkszaehler_config.get('disabled', False):
        from .outputs import VolkszaehlerOutputPlugin
        volkszaehler_client = VolkszaehlerOutputPlugin(
                volkszaehler_config)

    g_inverters = [g_inverter.get('serial') for g_inverter in ahoy_config.get('inverters', [])]
    for g_inverter in ahoy_config.get('inverters', []):
        g_inverter_ser = g_inverter.get('serial')
        inv_str = str(g_inverter_ser)
        command_queue[inv_str] = []
        event_message_index[inv_str] = 0

        #
        # Enables and subscribe inverter to mqtt /command-Topic
        #
        if mqtt_client and g_inverter.get('mqtt', {}).get('send_raw_enabled', False):
            topic_item = (
                    str(g_inverter_ser),
                    g_inverter.get('mqtt', {}).get('topic', f'hoymiles/{g_inverter_ser}') + '/command'
                    )
            mqtt_client.subscribe(topic_item[1])
            mqtt_command_topic_subs.append(topic_item)

    logging.info(f'Starting main_loop with inverter(s) {g_inverters}')
    main_loop(ahoy_config)
