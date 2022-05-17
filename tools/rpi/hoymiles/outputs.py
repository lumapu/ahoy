#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Hoymiles output plugin library
"""

import socket
from datetime import datetime, timezone
from hoymiles.decoders import StatusResponse

try:
    from influxdb_client import InfluxDBClient
except ModuleNotFoundError:
    pass

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

        self._bucket = params.get('bucket', 'hoymiles/autogen')
        self._org = params.get('org', '')
        self._measurement = params.get('measurement',
                f'inverter,host={socket.gethostname()}')

        client = InfluxDBClient(url, token, bucket=self._bucket)
        self.api = client.write_api()

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

        # AC Data
        phase_id = 0
        for phase in data['phases']:
            data_stack.append(f'{measurement},phase={phase_id},type=power value={phase["power"]} {ctime}')
            data_stack.append(f'{measurement},phase={phase_id},type=voltage value={phase["voltage"]} {ctime}')
            data_stack.append(f'{measurement},phase={phase_id},type=current value={phase["current"]} {ctime}')
            phase_id = phase_id + 1

        # DC Data
        string_id = 0
        for string in data['strings']:
            data_stack.append(f'{measurement},string={string_id},type=total value={string["energy_total"]/1000:.4f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=power value={string["power"]:.2f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=voltage value={string["voltage"]:.3f} {ctime}')
            data_stack.append(f'{measurement},string={string_id},type=current value={string["current"]:3f} {ctime}')
            string_id = string_id + 1
        # Global
        data_stack.append(f'{measurement},type=frequency value={data["frequency"]:.3f} {ctime}')
        data_stack.append(f'{measurement},type=temperature value={data["temperature"]:.2f} {ctime}')

        self.api.write(self._bucket, self._org, data_stack)

try:
    import paho.mqtt.client
except ModuleNotFoundError:
    pass

class MqttOutputPlugin(OutputPluginFactory):
    """ Mqtt output plugin """
    client = None

    def __init__(self, *args, **params):
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
        super().__init__(*args, **params)

        mqtt_client = paho.mqtt.client.Client()
        mqtt_client.username_pw_set(params.get('user', None), params.get('password', None))
        mqtt_client.connect(params.get('host', '127.0.0.1'), params.get('port', 1883))
        mqtt_client.loop_start()

        self.client = mqtt_client

    def store_status(self, response, **params):
        """
        Publish StatusResponse object

        :param hoymiles.decoders.StatusResponse response: StatusResponse object
        :param topic: custom mqtt topic prefix (default: hoymiles/{inverter_ser})
        :type topic: str

        :raises ValueError: when response is not instance of StatusResponse
        """

        if not isinstance(response, StatusResponse):
            raise ValueError('Data needs to be instance of StatusResponse')

        data = response.__dict__()

        topic = params.get('topic', f'hoymiles/{data["inverter_ser"]}')

        # AC Data
        phase_id = 0
        for phase in data['phases']:
            self.client.publish(f'{topic}/emeter/{phase_id}/power', phase['power'])
            self.client.publish(f'{topic}/emeter/{phase_id}/voltage', phase['voltage'])
            self.client.publish(f'{topic}/emeter/{phase_id}/current', phase['current'])
            phase_id = phase_id + 1

        # DC Data
        string_id = 0
        for string in data['strings']:
            self.client.publish(f'{topic}/emeter-dc/{string_id}/total', string['energy_total']/1000)
            self.client.publish(f'{topic}/emeter-dc/{string_id}/power', string['power'])
            self.client.publish(f'{topic}/emeter-dc/{string_id}/voltage', string['voltage'])
            self.client.publish(f'{topic}/emeter-dc/{string_id}/current', string['current'])
            string_id = string_id + 1
        # Global
        self.client.publish(f'{topic}/frequency', data['frequency'])
        self.client.publish(f'{topic}/temperature', data['temperature'])
