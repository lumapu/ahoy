#!/usr/bin/python3
# -*- coding: utf-8 -*-

"""
Hoymiles Micro-Inverters decoder library
"""

import struct
from datetime import datetime, timedelta
import crcmod

f_crc_m = crcmod.predefined.mkPredefinedCrcFun('modbus')

class Response:
    """ All Response Shared methods """
    inverter_ser = None
    inverter_name = None
    dtu_ser = None
    response = None

    def __init__(self, *args, **params):
        """
        :param bytes response: response payload bytes
        """
        self.inverter_ser = params.get('inverter_ser', None)
        self.inverter_name = params.get('inverter_name', None)
        self.dtu_ser = params.get('dtu_ser', None)

        self.response = args[0]

        if isinstance(params.get('time_rx', None), datetime):
            self.time_rx = params['time_rx']
        else:
            self.time_rx = datetime.now()

    def __dict__(self):
        """ Base values, availabe in each __dict__ call """
        return {
                'inverter_ser': self.inverter_ser,
                'inverter_name': self.inverter_name,
                'dtu_ser': self.dtu_ser}

class StatusResponse(Response):
    """Inverter StatusResponse object"""
    e_keys  = ['voltage','current','power','energy_total','energy_daily','powerfactor']
    temperature = None
    frequency = None

    def unpack(self, fmt, base):
        """
        Data unpack helper

        :param str fmt: struct format string
        :param int base: unpack base position from self.response bytes
        :return: unpacked values
        :rtype: tuple
        """
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.response[base:base+size])

    @property
    def phases(self):
        """
        AC power data

        :retrun: list of dict's
        :rtype: list
        """
        phases = []
        p_exists = True
        while p_exists:
            p_exists = False
            phase_id = len(phases)
            phase = {}
            for key in self.e_keys:
                prop = f'ac_{key}_{phase_id}'
                if hasattr(self, prop):
                    p_exists = True
                    phase[key] = getattr(self, prop)
            if p_exists:
                phases.append(phase)

        return phases

    @property
    def strings(self):
        """
        DC PV-string data

        :retrun: list of dict's
        :rtype: list
        """
        strings = []
        s_exists = True
        while s_exists:
            s_exists = False
            string_id = len(strings)
            string = {}
            for key in self.e_keys:
                prop = f'dc_{key}_{string_id}'
                if hasattr(self, prop):
                    s_exists = True
                    string[key] = getattr(self, prop)
            if s_exists:
                strings.append(string)

        return strings

    def __dict__(self):
        """
        Get all known data

        :return: dict of properties
        :rtype: dict
        """
        data = super().__dict__()
        data['phases'] = self.phases
        data['strings'] = self.strings
        data['temperature'] = self.temperature
        data['frequency'] = self.frequency
        data['time'] = self.time_rx
        return data

class UnknownResponse(Response):
    """
    Debugging helper for unknown payload format
    """

    @property
    def hex_ascii(self):
        """
        Generate white-space separated byte representation

        :return: hexlifierd byte string
        :rtype: str
        """
        return ' '.join([f'{byte:02x}' for byte in self.response])

    @property
    def valid_crc(self):
        """
        Checks if self.response has valid Modbus CRC

        :return: if crc is available and correct
        :rtype: bool
        """
        # check crc
        pcrc = struct.unpack('>H', self.response[-2:])[0]
        return f_crc_m(self.response[:-2]) == pcrc

    @property
    def dump_longs(self):
        """Get all data, interpreted as long"""
        if len(self.response) < 3:
            return None

        res = self.response

        rem = len(res) % 16
        res = res[:rem*-1]

        vals = None
        if len(res) % 16 == 0:
            rlen = len(res)/4
            vals = struct.unpack(f'>{int(rlen)}L', res)

        return vals

    @property
    def dump_longs_pad1(self):
        """Get all data, interpreted as long"""
        if len(self.response) < 5:
            return None

        res = self.response[2:]

        rem = len(res) % 16
        res = res[:rem*-1]

        vals = None
        if len(res) % 16 == 0:
            rlen = len(res)/4
            vals = struct.unpack(f'>{int(rlen)}L', res)

        return vals

    @property
    def dump_longs_pad2(self):
        """Get all data, interpreted as long"""
        if len(self.response) < 7:
            return None

        res = self.response[4:]

        rem = len(res) % 16
        res = res[:rem*-1]

        vals = None
        if len(res) % 16 == 0:
            rlen = len(res)/4
            vals = struct.unpack(f'>{int(rlen)}L', res)

        return vals

    @property
    def dump_longs_pad3(self):
        """Get all data, interpreted as long"""
        if len(self.response) < 9:
            return None

        res = self.response[6:]

        rem = len(res) % 16
        res = res[:rem*-1]

        vals = None
        if len(res) % 16 == 0:
            rlen = len(res)/4
            vals = struct.unpack(f'>{int(rlen)}L', res)

        return vals

    @property
    def dump_shorts(self):
        """Get all data, interpreted as short"""
        if len(self.response) < 3:
            return None

        res = self.response

        rem = len(res) % 4
        res = res[:rem*-1]

        vals = None
        if len(res) % 4 == 0:
            rlen = len(res)/2
            vals = struct.unpack(f'>{int(rlen)}H', res)

        return vals

    @property
    def dump_shorts_pad1(self):
        """Get all data, interpreted as short"""
        if len(self.response) < 4:
            return None

        res = self.response[1:]

        rem = len(res) % 4
        res = res[:rem*-1]

        vals = None
        if len(res) % 4 == 0:
            rlen = len(res)/2
            vals = struct.unpack(f'>{int(rlen)}H', res)

        return vals

class EventsResponse(UnknownResponse):
    """ Hoymiles micro-inverter event log decode helper """

    alarm_codes = {
            1: 'Inverter start',
            2: 'DTU command failed',
            121: 'Over temperature protection',
            125: 'Grid configuration parameter error',
            126: 'Software error code 126',
            127: 'Firmware error',
            128: 'Software error code 128',
            129: 'Software error code 129',
            130: 'Offline',
            141: 'Grid overvoltage',
            142: 'Average grid overvoltage',
            143: 'Grid undervoltage',
            144: 'Grid overfrequency',
            145: 'Grid underfrequency',
            146: 'Rapid grid frequency change',
            147: 'Power grid outage',
            148: 'Grid disconnection',
            149: 'Island detected',
            205: 'Input port 1 & 2 overvoltage',
            206: 'Input port 3 & 4 overvoltage',
            207: 'Input port 1 & 2 undervoltage',
            208: 'Input port 3 & 4 undervoltage',
            209: 'Port 1 no input',
            210: 'Port 2 no input',
            211: 'Port 3 no input',
            212: 'Port 4 no input',
            213: 'PV-1 & PV-2 abnormal wiring',
            214: 'PV-3 & PV-4 abnormal wiring',
            215: 'PV-1 Input overvoltage',
            216: 'PV-1 Input undervoltage',
            217: 'PV-2 Input overvoltage',
            218: 'PV-2 Input undervoltage',
            219: 'PV-3 Input overvoltage',
            220: 'PV-3 Input undervoltage',
            221: 'PV-4 Input overvoltage',
            222: 'PV-4 Input undervoltage',
            301: 'Hardware error code 301',
            302: 'Hardware error code 302',
            303: 'Hardware error code 303',
            304: 'Hardware error code 304',
            305: 'Hardware error code 305',
            306: 'Hardware error code 306',
            307: 'Hardware error code 307',
            308: 'Hardware error code 308',
            309: 'Hardware error code 309',
            310: 'Hardware error code 310',
            311: 'Hardware error code 311',
            312: 'Hardware error code 312',
            313: 'Hardware error code 313',
            314: 'Hardware error code 314',
            5041: 'Error code-04 Port 1',
            5042: 'Error code-04 Port 2',
            5043: 'Error code-04 Port 3',
            5044: 'Error code-04 Port 4',
            5051: 'PV Input 1 Overvoltage/Undervoltage',
            5052: 'PV Input 2 Overvoltage/Undervoltage',
            5053: 'PV Input 3 Overvoltage/Undervoltage',
            5054: 'PV Input 4 Overvoltage/Undervoltage',
            5060: 'Abnormal bias',
            5070: 'Over temperature protection',
            5080: 'Grid Overvoltage/Undervoltage',
            5090: 'Grid Overfrequency/Underfrequency',
            5100: 'Island detected',
            5120: 'EEPROM reading and writing error',
            5150: '10 min value grid overvoltage',
            5200: 'Firmware error',
            8310: 'Shut down',
            9000: 'Microinverter is suspected of being stolen'
            }

    def __init__(self, *args, **params):
        super().__init__(*args, **params)

        crc_valid = self.valid_crc
        if crc_valid:
            print(' payload has valid modbus crc')
            self.response = self.response[:-2]

        status = self.response[:2]

        chunk_size = 12
        for i_chunk in range(2, len(self.response), chunk_size):
            chunk = self.response[i_chunk:i_chunk+chunk_size]

            print(' '.join([f'{byte:02x}' for byte in chunk]) + ': ')

            opcode, a_code, a_count, uptime_sec = struct.unpack('>BBHH', chunk[0:6])
            a_text = self.alarm_codes.get(a_code, 'N/A')

            print(f' uptime={timedelta(seconds=uptime_sec)} a_count={a_count} opcode={opcode} a_code={a_code} a_text={a_text}')

            for fmt in ['BBHHHHH']:
                print(f' {fmt:7}: ' + str(struct.unpack('>' + fmt, chunk)))
            print(end='', flush=True)

class DebugDecodeAny(UnknownResponse):
    """Default decoder"""

    def __init__(self, *args, **params):
        super().__init__(*args, **params)

        crc_valid = self.valid_crc
        if crc_valid:
            print(' payload has valid modbus crc')
            self.response = self.response[:-2]

        l_payload = len(self.response)
        print(f' payload has {l_payload} bytes')

        longs = self.dump_longs
        if not longs:
            print(' type long      : unable to decode (len or not mod 4)')
        else:
            print(' type long      : ' + str(longs))

        longs = self.dump_longs_pad1
        if not longs:
            print(' type long pad1 : unable to decode (len or not mod 4)')
        else:
            print(' type long pad1 : ' + str(longs))

        longs = self.dump_longs_pad2
        if not longs:
            print(' type long pad2 : unable to decode (len or not mod 4)')
        else:
            print(' type long pad2 : ' + str(longs))

        longs = self.dump_longs_pad3
        if not longs:
            print(' type long pad3 : unable to decode (len or not mod 4)')
        else:
            print(' type long pad3 : ' + str(longs))

        shorts = self.dump_shorts
        if not shorts:
            print(' type short     : unable to decode (len or not mod 2)')
        else:
            print(' type short     : ' + str(shorts))

        shorts = self.dump_shorts_pad1
        if not shorts:
            print(' type short pad1: unable to decode (len or not mod 2)')
        else:
            print(' type short pad1: ' + str(shorts))

        try:
            if len(self.response) > 2:
                print(' type utf-8  : ' + self.response.decode('utf-8'))
        except UnicodeDecodeError:
            print(' type utf-8  : utf-8 decode error')

        try:
            if len(self.response) > 2:
                print(' type ascii  : ' + self.response.decode('ascii'))
        except UnicodeDecodeError:
            print(' type ascii  : ascii decode error')


# 1121-Series Intervers, 1 MPPT, 1 Phase
class Hm300Decode0B(StatusResponse):
    """ 1121-series mirco-inverters status data """

    @property
    def dc_voltage_0(self):
        """ String 1 VDC """
        return self.unpack('>H', 2)[0]/10
    @property
    def dc_current_0(self):
        """ String 1 ampere """
        return self.unpack('>H', 4)[0]/100
    @property
    def dc_power_0(self):
        """ String 1 watts """
        return self.unpack('>H', 6)[0]/10
    @property
    def dc_energy_total_0(self):
        """ String 1 total energy in Wh """
        return self.unpack('>L', 8)[0]
    @property
    def dc_energy_daily_0(self):
        """ String 1 daily energy in Wh """
        return self.unpack('>H', 12)[0]

    @property
    def ac_voltage_0(self):
        """ Phase 1 VAC """
        return self.unpack('>H', 14)[0]/10
    @property
    def ac_current_0(self):
        """ Phase 1 ampere """
        return self.unpack('>H', 22)[0]/100
    @property
    def ac_power_0(self):
        """ Phase 1 watts """
        return self.unpack('>H', 18)[0]/10
    @property
    def frequency(self):
        """ Grid frequency in Hertz """
        return self.unpack('>H', 16)[0]/100
    @property
    def temperature(self):
        """ Inverter temperature in °C """
        return self.unpack('>H', 26)[0]/10

class Hm300Decode11(EventsResponse):
    """ Inverter generic events log """

class Hm300Decode12(EventsResponse):
    """ Inverter major events log """


# 1141-Series Inverters, 2 MPPT, 1 Phase
class Hm600Decode0B(StatusResponse):
    """ 1141-series mirco-inverters status data """

    @property
    def dc_voltage_0(self):
        """ String 1 VDC """
        return self.unpack('>H', 2)[0]/10
    @property
    def dc_current_0(self):
        """ String 1 ampere """
        return self.unpack('>H', 4)[0]/100
    @property
    def dc_power_0(self):
        """ String 1 watts """
        return self.unpack('>H', 6)[0]/10
    @property
    def dc_energy_total_0(self):
        """ String 1 total energy in Wh """
        return self.unpack('>L', 14)[0]
    @property
    def dc_energy_daily_0(self):
        """ String 1 daily energy in Wh """
        return self.unpack('>H', 22)[0]

    @property
    def dc_voltage_1(self):
        """ String 2 VDC """
        return self.unpack('>H', 8)[0]/10
    @property
    def dc_current_1(self):
        """ String 2 ampere """
        return self.unpack('>H', 10)[0]/100
    @property
    def dc_power_1(self):
        """ String 2 watts """
        return self.unpack('>H', 12)[0]/10
    @property
    def dc_energy_total_1(self):
        """ String 2 total energy in Wh """
        return self.unpack('>L', 18)[0]
    @property
    def dc_energy_daily_1(self):
        """ String 2 daily energy in Wh """
        return self.unpack('>H', 24)[0]

    @property
    def ac_voltage_0(self):
        """ Phase 1 VAC """
        return self.unpack('>H', 26)[0]/10
    @property
    def ac_current_0(self):
        """ Phase 1 ampere """
        return self.unpack('>H', 34)[0]/10
    @property
    def ac_power_0(self):
        """ Phase 1 watts """
        return self.unpack('>H', 30)[0]/10
    @property
    def frequency(self):
        """ Grid frequency in Hertz """
        return self.unpack('>H', 28)[0]/100
    @property
    def temperature(self):
        """ Inverter temperature in °C """
        return self.unpack('>H', 38)[0]/10
    @property
    def alarm_count(self):
        """ Event counter """
        return self.unpack('>H', 40)[0]

class Hm600Decode11(EventsResponse):
    """ Inverter generic events log """

class Hm600Decode12(EventsResponse):
    """ Inverter major events log """


# 1161-Series Inverters, 4 MPPT, 1 Phase
class Hm1200Decode0B(StatusResponse):
    """ 1161-series mirco-inverters status data """

    @property
    def dc_voltage_0(self):
        """ String 1 VDC """
        return self.unpack('>H', 2)[0]/10
    @property
    def dc_current_0(self):
        """ String 1 ampere """
        return self.unpack('>H', 4)[0]/100
    @property
    def dc_power_0(self):
        """ String 1 watts """
        return self.unpack('>H', 8)[0]/10
    @property
    def dc_energy_total_0(self):
        """ String 1 total energy in Wh """
        return self.unpack('>L', 12)[0]
    @property
    def dc_energy_daily_0(self):
        """ String 1 daily energy in Wh """
        return self.unpack('>H', 20)[0]

    @property
    def dc_voltage_1(self):
        """ String 2 VDC """
        return self.unpack('>H', 2)[0]/10
    @property
    def dc_current_1(self):
        """ String 2 ampere """
        return self.unpack('>H', 4)[0]/100
    @property
    def dc_power_1(self):
        """ String 2 watts """
        return self.unpack('>H', 10)[0]/10
    @property
    def dc_energy_total_1(self):
        """ String 2 total energy in Wh """
        return self.unpack('>L', 16)[0]
    @property
    def dc_energy_daily_1(self):
        """ String 2 daily energy in Wh """
        return self.unpack('>H', 22)[0]

    @property
    def dc_voltage_2(self):
        """ String 3 VDC """
        return self.unpack('>H', 24)[0]/10
    @property
    def dc_current_2(self):
        """ String 3 ampere """
        return self.unpack('>H', 26)[0]/100
    @property
    def dc_power_2(self):
        """ String 3 watts """
        return self.unpack('>H', 30)[0]/10
    @property
    def dc_energy_total_2(self):
        """ String 3 total energy in Wh """
        return self.unpack('>L', 34)[0]
    @property
    def dc_energy_daily_2(self):
        """ String 3 daily energy in Wh """
        return self.unpack('>H', 42)[0]

    @property
    def dc_voltage_3(self):
        """ String 4 VDC """
        return self.unpack('>H', 24)[0]/10
    @property
    def dc_current_3(self):
        """ String 4 ampere """
        return self.unpack('>H', 28)[0]/100
    @property
    def dc_power_3(self):
        """ String 4 watts """
        return self.unpack('>H', 32)[0]/10
    @property
    def dc_energy_total_3(self):
        """ String 4 total energy in Wh """
        return self.unpack('>L', 38)[0]
    @property
    def dc_energy_daily_3(self):
        """ String 4 daily energy in Wh """
        return self.unpack('>H', 44)[0]

    @property
    def ac_voltage_0(self):
        """ Phase 1 VAC """
        return self.unpack('>H', 46)[0]/10
    @property
    def ac_current_0(self):
        """ Phase 1 ampere """
        return self.unpack('>H', 54)[0]/100
    @property
    def ac_power_0(self):
        """ Phase 1 watts """
        return self.unpack('>H', 50)[0]/10
    @property
    def frequency(self):
        """ Grid frequency in Hertz """
        return self.unpack('>H', 48)[0]/100
    @property
    def temperature(self):
        """ Inverter temperature in °C """
        return self.unpack('>H', 58)[0]/10

class Hm1200Decode11(EventsResponse):
    """ Inverter generic events log """

class Hm1200Decode12(EventsResponse):
    """ Inverter major events log """
