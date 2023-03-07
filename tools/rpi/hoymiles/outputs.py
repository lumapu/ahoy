#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Hoymiles output plugin library
"""

import socket
import logging
from datetime import datetime, timezone
from hoymiles.decoders import StatusResponse, HardwareInfoResponse
from hoymiles import HOYMILES_TRANSACTION_LOGGING, HOYMILES_DEBUG_LOGGING

class OutputPluginFactory:
    def __init__(self, **params):
        """
        Initialize output plugin

        :param inverter_ser: The inverter serial
        :type inverter_ser: str
        :param inverter_name: The configured name for the inverter
        :type inverter_name: str
        """

        self.inverter_ser = params.get('inverter_ser', '')
        self.inverter_name = params.get('inverter_name', None)

    def store_status(self, response, **params):
        """
        Default function

        :raises NotImplementedError: when the plugin does not implement store status data
        """
        raise NotImplementedError('The current output plugin does not implement store_status')

class InfluxOutputPlugin(OutputPluginFactory):
    """ Influx2 output plugin """
    api = None

    def __init__(self, url, token, **params):
        """
        Initialize InfluxOutputPlugin
        https://influxdb-client.readthedocs.io/en/stable/api.html#influxdbclient

        The following targets must be present in your InfluxDB. This does not
        automatically create anything for You.

        :param str url: The url to connect this client to. Like http://localhost:8086
        :param str token: Influx2 access token which is allowed to write to bucket
        :param org: Influx2 org, the token belongs to
        :type org: str
        :param bucket: Influx2 bucket to store data in (also known as retention policy)
        :type bucket: str
        :param measurement: Default measurement-prefix to use
        :type measurement: str
        """
        super().__init__(**params)

        try:
            from influxdb_client import InfluxDBClient
        except ModuleNotFoundError:
            ErrorText1 = f'Module "influxdb_client" for INFLUXDB necessary.'
            ErrorText2 = f'Install module with command: python3 -m pip install influxdb_client'
            print(ErrorText1, ErrorText2)
            logging.error(ErrorText1)
            logging.error(ErrorText2)
            exit()

        self._bucket = params.get('bucket', 'hoymiles/autogen')
        self._org = params.get('org', '')
        self._measurement = params.get('measurement', f'inverter,host={socket.gethostname()}')

        with InfluxDBClient(url, token, bucket=self._bucket) as self.client:
             self.api = self.client.write_api()

    def disco(self, **params):
        self.client.close()          # Shutdown the client
        return

    def store_status(self, response, **params):
        """
        Publish StatusResponse object

        :param hoymiles.decoders.StatusResponse response: StatusResponse object
        :type response: hoymiles.decoders.StatusResponse
        :param measurement: Custom influx measurement name
        :type measurement: str or None

        :raises ValueError: when response is not instance of StatusResponse
        """

        if not isinstance(response, StatusResponse):
            raise ValueError('Data needs to be instance of StatusResponse')

        data = response.__dict__()

        measurement = self._measurement + f',location={data["inverter_ser"]}'

        data_stack = []

        time_rx = datetime.now()
        if 'time' in data and isinstance(data['time'], datetime):
            time_rx = data['time']

        # InfluxDB uses UTC
        utctime = datetime.fromtimestamp(time_rx.timestamp(), tz=timezone.utc)

        # InfluxDB requires nanoseconds
        ctime = int(utctime.timestamp() * 1e9)

        if HOYMILES_DEBUG_LOGGING:
            logging.info(f'InfluxDB: utctime: {utctime}')

        # AC Data
        phase_id = 0
        for phase in data['phases']:
            data_stack.append(f'{measurement},phase={phase_id},type=voltage value={phase["voltage"]} {ctime}')
            data_stack.append(f'{measurement},phase={phase_id},type=current value={phase["current"]} {ctime}')
            data_stack.append(f'{measurement},phase={phase_id},type=power value={phase["power"]} {ctime}')
            data_stack.append(f'{measurement},phase={phase_id},type=Q_AC value={phase["reactive_power"]} {ctime}')
            data_stack.append(f'{measurement},phase={phase_id},type=frequency value={phase["frequency"]:.3f} {ctime}')
            phase_id = phase_id + 1

        # DC Data
        string_id = 0
        for string in data['strings']:
            data_stack.append(f'{measurement},string={string_id},type=voltage value={string["voltage"]:.3f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=current value={string["current"]:3f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=power value={string["power"]:.2f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=YieldDay value={string["energy_daily"]:.2f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=YieldTotal value={string["energy_total"]/1000:.4f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=Irradiation value={string["irradiation"]:.2f} {ctime}')
            string_id = string_id + 1

        # Global
        if data['event_count'] is not None:
            data_stack.append(f'{measurement},type=total_events value={data["event_count"]} {ctime}')
        if data['powerfactor'] is not None:
            data_stack.append(f'{measurement},type=PF_AC value={data["powerfactor"]:f} {ctime}')
        data_stack.append(f'{measurement},type=Temp value={data["temperature"]:.2f} {ctime}')
        if data['yield_total'] is not None:
            data_stack.append(f'{measurement},type=YieldTotal value={data["yield_total"]/1000:.3f} {ctime}')
        if data['yield_today'] is not None:
            data_stack.append(f'{measurement},type=YieldToday value={data["yield_today"]/1000:.3f} {ctime}')
        data_stack.append(f'{measurement},type=Efficiency value={data["efficiency"]:.2f} {ctime}')

        if HOYMILES_DEBUG_LOGGING:
            #logging.debug(f'INFLUX data to DB: {data_stack}')
            pass
        self.api.write(self._bucket, self._org, data_stack)

class MqttOutputPlugin(OutputPluginFactory):
    """ Mqtt output plugin """
    client = None

    def __init__(self, config, **params):
        """
        Initialize MqttOutputPlugin

        :param host: Broker ip or hostname (defaults to: 127.0.0.1)
        :type host: str
        :param port: Broker port
        :type port: int (defaults to: 1883)
        :param user: Optional username to login to the broker
        :type user: str or None
        :param password: Optional passwort to login to the broker
        :type password: str or None
        :param topic: Topic prefix to use (defaults to: hoymiles/{inverter_ser})
        :type topic: str

        :param paho.mqtt.client.Client broker: mqtt-client instance
        :param str inverter_ser: inverter serial
        :param hoymiles.StatusResponse data: decoded inverter StatusResponse
        :param topic: custom mqtt topic prefix (default: hoymiles/{inverter_ser})
        :type topic: str
        """
        super().__init__(**params)

        try:
            import paho.mqtt.client
        except ModuleNotFoundError:
            ErrorText1 = f'Module "paho.mqtt.client" for MQTT-output necessary.'
            ErrorText2 = f'Install module with command: python3 -m pip install paho-mqtt'
            print(ErrorText1, ErrorText2)
            logging.error(ErrorText1)
            logging.error(ErrorText2)
            exit()

        mqtt_client = paho.mqtt.client.Client()
        if config.get('useTLS',False):
           mqtt_client.tls_set()
           mqtt_client.tls_insecure_set(config.get('insecureTLS',False))
        mqtt_client.username_pw_set(config.get('user', None), config.get('password', None))

        last_will = config.get('last_will', None)
        if last_will:
            lw_topic = last_will.get('topic', 'last will hoymiles')
            lw_payload = last_will.get('payload', 'last will')
            mqtt_client.will_set(str(lw_topic), str(lw_payload))

        mqtt_client.connect(config.get('host', '127.0.0.1'), config.get('port', 1883))
        mqtt_client.loop_start()

        self.client = mqtt_client
        self.qos = config.get('QoS', 0)         # Quality of Service
        self.ret = config.get('Retain', True)   # Retain Message

    def disco(self, **params):
        self.client.loop_stop()    # Stop loop 
        self.client.disconnect()   # disconnect
        return

    def info2mqtt(self, mqtt_topic, mqtt_data):
        for mqtt_key in mqtt_data:
            self.client.publish(f'{mqtt_topic["topic"]}/{mqtt_key}', mqtt_data[mqtt_key], self.qos, self.ret)
        return

    def store_status(self, response, **params):
        """
        Publish StatusResponse object

        :param hoymiles.decoders.StatusResponse response: StatusResponse object
        :param topic: custom mqtt topic prefix (default: hoymiles/{inverter_ser})
        :type topic: str

        :raises ValueError: when response is not instance of StatusResponse
        """

        data = response.__dict__()
        topic = params.get('topic', None)
        if not topic:
            topic = f'{data.get("inverter_name", "hoymiles")}/{data.get("inverter_ser", None)}'

        if HOYMILES_DEBUG_LOGGING:
            logging.info(f'MQTT-topic: {topic} data-type: {type(response)}')

        if isinstance(response, StatusResponse):

            # Global Head
            if data['time'] is not None:
               self.client.publish(f'{topic}/time', data['time'].strftime("%d.%m.%YT%H:%M:%S"), self.qos, self.ret)

            # AC Data
            phase_id = 0
            phase_sum_power = 0
            for phase in data['phases']:
                self.client.publish(f'{topic}/emeter/{phase_id}/voltage', phase['voltage'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter/{phase_id}/current', phase['current'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter/{phase_id}/power', phase['power'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter/{phase_id}/Q_AC', phase['reactive_power'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter/{phase_id}/frequency', phase['frequency'], self.qos, self.ret)
                phase_id = phase_id + 1
                phase_sum_power += phase['power']

            # DC Data
            string_id = 0
            string_sum_power = 0
            for string in data['strings']:
                if 'name' in string:
                    string_name = string['name'].replace(" ","_")
                else:
                    string_name = string_id
                self.client.publish(f'{topic}/emeter-dc/{string_name}/voltage', string['voltage'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter-dc/{string_name}/current', string['current'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter-dc/{string_name}/power', string['power'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter-dc/{string_name}/YieldDay', string['energy_daily'], self.qos, self.ret)
                self.client.publish(f'{topic}/emeter-dc/{string_name}/YieldTotal', string['energy_total']/1000, self.qos, self.ret)
                self.client.publish(f'{topic}/emeter-dc/{string_name}/Irradiation', string['irradiation'], self.qos, self.ret)
                string_id = string_id + 1
                string_sum_power += string['power']

            # Global
            if data['event_count'] is not None:
               self.client.publish(f'{topic}/total_events', data['event_count'], self.qos, self.ret)
            if data['powerfactor'] is not None:
               self.client.publish(f'{topic}/PF_AC', data['powerfactor'], self.qos, self.ret)
            self.client.publish(f'{topic}/Temp', data['temperature'], self.qos, self.ret)
            if data['yield_total'] is not None:
               self.client.publish(f'{topic}/YieldTotal', data['yield_total']/1000, self.qos, self.ret)
            if data['yield_today'] is not None:
               self.client.publish(f'{topic}/YieldToday', data['yield_today']/1000, self.qos, self.ret)
            self.client.publish(f'{topic}/Efficiency', data['efficiency'], self.qos, self.ret)


        elif isinstance(response, HardwareInfoResponse):
            self.client.publish(f'{topic}/Firmware/Version',\
                 f'{data["FW_ver_maj"]}.{data["FW_ver_min"]}.{data["FW_ver_pat"]}', self.qos, self.ret)

            self.client.publish(f'{topic}/Firmware/Build_at',\
                 f'{data["FW_build_dd"]}/{data["FW_build_mm"]}/{data["FW_build_yy"]}T{data["FW_build_HH"]}:{data["FW_build_MM"]}',\
                 self.qos, self.ret)

            self.client.publish(f'{topic}/Firmware/HWPartId',\
                 f'{data["FW_HW_ID"]}', self.qos, self.ret)

        else:
             raise ValueError('Data needs to be instance of StatusResponse or a instance of HardwareInfoResponse')

class VzInverterOutput:
    def __init__(self, config, session):
        self.session = session
        self.serial = config.get('serial')
        self.baseurl = config.get('url', 'http://localhost/middleware/')
        self.channels = dict()

        for channel in config.get('channels', []):
            uid = channel.get('uid', None)
            ctype = channel.get('type')
            # if uid and ctype:
            if ctype:
                self.channels[ctype] = uid

    def store_status(self, data, session):
        """
        Publish StatusResponse object

        :param hoymiles.decoders.StatusResponse response: StatusResponse object

        :raises ValueError: when response is not instance of StatusResponse
        """
        if len(self.channels) == 0:
            return

        ts = int(round(data['time'].timestamp() * 1000))

        if HOYMILES_DEBUG_LOGGING:
            logging.info(f'Volkszaehler-Timestamp: {ts}')

        # AC Data
        phase_id = 0
        for phase in data['phases']:
            self.try_publish(ts, f'ac_voltage{phase_id}', phase['voltage'])
            self.try_publish(ts, f'ac_current{phase_id}', phase['current'])
            self.try_publish(ts, f'ac_power{phase_id}', phase['power'])
            self.try_publish(ts, f'ac_reactive_power{phase_id}', phase['reactive_power'])
            self.try_publish(ts, f'ac_frequency{phase_id}', phase['frequency'])
            phase_id = phase_id + 1

        # DC Data
        string_id = 0
        for string in data['strings']:
            self.try_publish(ts, f'dc_voltage{string_id}', string['voltage'])
            self.try_publish(ts, f'dc_current{string_id}', string['current'])
            self.try_publish(ts, f'dc_power{string_id}', string['power'])
            self.try_publish(ts, f'dc_energy_daily{string_id}', string['energy_daily'])
            self.try_publish(ts, f'dc_energy_total{string_id}', string['energy_total'])
            self.try_publish(ts, f'dc_irradiation{string_id}', string['irradiation'])
            string_id = string_id + 1

        # Global
        if data['event_count'] is not None:
            self.try_publish(ts, f'event_count', data['event_count'])
        if data['powerfactor'] is not None:
            self.try_publish(ts, f'powerfactor', data['powerfactor'])
        self.try_publish(ts, f'temperature', data['temperature'])
        if data['yield_total'] is not None:
            self.try_publish(ts, f'yield_total', data['yield_total'])
        if data['yield_today'] is not None:
            self.try_publish(ts, f'yield_today', data['yield_today'])
        self.try_publish(ts, f'efficiency', data['efficiency'])
        return

    def try_publish(self, ts, ctype, value):
        if not ctype in self.channels:
            if HOYMILES_DEBUG_LOGGING:
                logging.warning(f'ctype \"{ctype}\" not found in ahoy.yml')
            return

        uid = self.channels[ctype]
        url = f'{self.baseurl}/data/{uid}.json?operation=add&ts={ts}&value={value}'
        if uid == None:
            if HOYMILES_DEBUG_LOGGING:
                logging.debug(f'ctype \"{ctype}\" has no configured uid-value in ahoy.yml')
            return

        if HOYMILES_DEBUG_LOGGING:
            logging.debug(f'VZ-url: {url}')

        try:
            r = self.session.get(url)
            if r.status_code == 404:
                logging.critical('VZ-DB not reachable, please check "middleware"')
            if r.status_code == 400:
                logging.critical('UUID not configured in VZ-DB')
            elif r.status_code != 200:
               raise ValueError(f'Transmit result {url}')
        except ConnectionError as e:
            raise ValueError(f'Could not connect VZ-DB {type(e)} {e.keys()}')
        return

class VolkszaehlerOutputPlugin(OutputPluginFactory):
    def __init__(self, config, **params):
        """
        Initialize VolkszaehlerOutputPlugin
        """
        super().__init__(**params)

        try:
            import requests
            import time
        except ModuleNotFoundError:
            ErrorText1 = f'Module "requests" and "time" for VolkszaehlerOutputPlugin necessary.'
            ErrorText2 = f'Install module with command: python3 -m pip install requests'
            print(ErrorText1, ErrorText2)
            logging.error(ErrorText1)
            logging.error(ErrorText2)
            exit(1)

        self.session = requests.Session()

        self.inverters = dict()
        for inverterconfig in config.get('inverters', []):
            serial = inverterconfig.get('serial')
            output = VzInverterOutput(inverterconfig, self.session)
            self.inverters[serial] = output

    def disco(self, **params):
        self.session.close()            # closing the connection
        return

    def store_status(self, response, **params):
        """
        Publish StatusResponse object

        :param hoymiles.decoders.StatusResponse response: StatusResponse object

        :raises ValueError: when response is not instance of StatusResponse
        """

        # check decoder object for output
        if not isinstance(response, StatusResponse):
            raise ValueError('Data needs to be instance of StatusResponse')

        if len(self.inverters) == 0:
            return

        data = response.__dict__()
        serial = data["inverter_ser"]
        if serial in self.inverters:
            output = self.inverters[serial]
            try:
                output.store_status(data, self.session)
            except ValueError as e:
                logging.warning('Could not send data to volkszaehler instance: %s' % e)
        return
