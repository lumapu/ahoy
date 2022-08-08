#!/usr/bin/python3
# -*- coding: utf-8 -*-

"""
Hoymiles Micro-Inverters decoder library
"""

import struct
from datetime import datetime, timedelta
import crcmod

f_crc_m = crcmod.predefined.mkPredefinedCrcFun('modbus')
f_crc8 = crcmod.mkCrcFun(0x101, initCrc=0, xorOut=0)

def g_unpack(s_fmt, s_buf):
    """Chunk unpack helper

    :param s_fmt: struct format string
    :type s_fmt: str
    :param s_buf: buffer to unpack
    :type s_buf: bytes
    :return: decoded data iterator
    :rtype: generator object
    """

    cs = struct.calcsize(s_fmt)
    s_exc = len(s_buf) % cs

    return struct.iter_unpack(s_fmt, s_buf[:len(s_buf) - s_exc])

def print_table_unpack(s_fmt, payload, cw=6):
    """
    Print table of decoded numbers with different offsets
    Helps recognizing values in unknown payloads

    :param s_fmt: struct format string
    :type s_fmt: str
    :param payload: bytes data
    :type payload: bytes
    :param cw: cell width
    :type cw: int
    :return: None
    """

    l_hexlified = [f'{byte:02x}' for byte in payload]

    print(f'{"Pos": <{cw}}', end='')
    print(''.join([f'{num: >{cw}}' for num in range(0, len(payload))]))
    print(f'{"Hex": <{cw}}', end='')
    print(''.join([f'{byte: >{cw}}' for byte in l_hexlified]))

    l_fmt = struct.calcsize(s_fmt)
    if len(payload) >= l_fmt:
        for offset in range(0, l_fmt):
            print(f'{s_fmt: <{cw}}', end='')
            print(' ' * cw * offset, end='')
            print(''.join(
                [f'{num[0]: >{cw*l_fmt}}' for num in g_unpack(s_fmt, payload[offset:])]))

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
    powerfactor = None
    event_count = None

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
        data['powerfactor'] = self.powerfactor
        data['event_count'] = self.event_count
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

    def validate_crc8(self):
        """
        Checks if self.response has valid CRC8

        :return: if crc is available and correct
        :rtype: bool
        """
        # check crc
        pcrc = struct.unpack('>B', self.response[-1:])[0]
        return f_crc8(self.response[:-1]) == pcrc

    def validate_crc_m(self):
        """
        Checks if self.response has valid Modbus CRC

        :return: if crc is available and correct
        :rtype: bool
        """
        # check crc
        pcrc = struct.unpack('>H', self.response[-2:])[0]
        return f_crc_m(self.response[:-2]) == pcrc

    def unpack_table(self, *args):
        """Access shared debug function"""
        print_table_unpack(*args)


class EventsResponse(UnknownResponse):
    """ Hoymiles micro-inverter event log decode helper """

    alarm_codes = {
            # HM Error Codes
            1: 'Inverter start', # 0x01
            2: 'DTU command failed', # 0x02
            121: 'Over temperature protection', # 0x79
            125: 'Grid configuration parameter error', # 0x7D
            126: 'Software error code 126', # 0x7E
            127: 'Firmware error', # 0x7F
            128: 'Software error code 128', # 0x80
            129: 'Software error code 129', # 0x81
            130: 'Offline', # 0x82
            141: 'Grid overvoltage', # 0x8D
            142: 'Average grid overvoltage', # 0x8E
            143: 'Grid undervoltage', # 0x8F
            144: 'Grid overfrequency', # 0x90
            145: 'Grid underfrequency', # 0x91
            146: 'Rapid grid frequency change', # 0x92
            147: 'Power grid outage', # 0x93
            148: 'Grid disconnection', # 0x94
            149: 'Island detected', # 0x95
            205: 'Input port 1 & 2 overvoltage', # 0xCD
            206: 'Input port 3 & 4 overvoltage', # 0xCE
            207: 'Input port 1 & 2 undervoltage', # 0xCF
            208: 'Input port 3 & 4 undervoltage', # 0xD0
            209: 'Port 1 no input', # 0xD1
            210: 'Port 2 no input', # 0xD2
            211: 'Port 3 no input', # 0xD3
            212: 'Port 4 no input', # 0xD4
            213: 'PV-1 & PV-2 abnormal wiring', # 0xD5
            214: 'PV-3 & PV-4 abnormal wiring', # 0xD6
            215: 'PV-1 Input overvoltage', # 0xD7
            216: 'PV-1 Input undervoltage', # 0xD8
            217: 'PV-2 Input overvoltage', # 0xD9
            218: 'PV-2 Input undervoltage', # 0xDA
            219: 'PV-3 Input overvoltage', # 0xDB
            220: 'PV-3 Input undervoltage', # 0xDC
            221: 'PV-4 Input overvoltage', # 0xDD
            222: 'PV-4 Input undervoltage', # 0xDE
            301: 'Hardware error code 301', # 0x012D
            302: 'Hardware error code 302', # 0x012E
            303: 'Hardware error code 303', # 0x012F
            304: 'Hardware error code 304', # 0x0130
            305: 'Hardware error code 305', # 0x0131
            306: 'Hardware error code 306', # 0x0132
            307: 'Hardware error code 307', # 0x0133
            308: 'Hardware error code 308', # 0x0134
            309: 'Hardware error code 309', # 0x0135
            310: 'Hardware error code 310', # 0x0136
            311: 'Hardware error code 311', # 0x0137
            312: 'Hardware error code 312', # 0x0138
            313: 'Hardware error code 313', # 0x0139
            314: 'Hardware error code 314', # 0x013A
            # MI Error Codes
            5041: 'Error code-04 Port 1', # 0x13B1
            5042: 'Error code-04 Port 2', # 0x13B2
            5043: 'Error code-04 Port 3', # 0x13B3
            5044: 'Error code-04 Port 4', # 0x13B4
            5051: 'PV Input 1 Overvoltage/Undervoltage', # 0x13BB
            5052: 'PV Input 2 Overvoltage/Undervoltage', # 0x13BC
            5053: 'PV Input 3 Overvoltage/Undervoltage', # 0x13BD
            5054: 'PV Input 4 Overvoltage/Undervoltage', # 0x13BE
            5060: 'Abnormal bias', # 0x13C4
            5070: 'Over temperature protection', # 0x13CE
            5080: 'Grid Overvoltage/Undervoltage', # 0x13D8
            5090: 'Grid Overfrequency/Underfrequency', # 0x13E2
            5100: 'Island detected', # 0x13EC
            5120: 'EEPROM reading and writing error', # 0x1400
            5150: '10 min value grid overvoltage', # 0x141E
            5200: 'Firmware error', # 0x1450
            8310: 'Shut down', # 0x2076
            9000: 'Microinverter is suspected of being stolen' # 0x2328
            }

    def __init__(self, *args, **params):
        super().__init__(*args, **params)

        crc_valid = self.validate_crc_m()
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

        crc8_valid = self.validate_crc8()
        if crc8_valid:
            print(' payload has valid crc8')
            self.response = self.response[:-1]

        crc_valid = self.validate_crc_m()
        if crc_valid:
            print(' payload has valid modbus crc')
            self.response = self.response[:-2]

        l_payload = len(self.response)
        print(f' payload has {l_payload} bytes')

        print()
        print('Field view: int')
        print_table_unpack('>B', self.response)

        print()
        print('Field view: shorts')
        print_table_unpack('>H', self.response)

        print()
        print('Field view: longs')
        print_table_unpack('>L', self.response)

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
class Hm300Decode02(EventsResponse):
    """ Inverter generic events log """

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
class Hm600Decode02(EventsResponse):
    """ Inverter generic events log """

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
    def powerfactor(self):
        """ Powerfactor """
        return self.unpack('>H', 36)[0]/1000
    @property
    def temperature(self):
        """ Inverter temperature in °C """
        return self.unpack('>H', 38)[0]/10
    @property
    def event_count(self):
        """ Event counter """
        return self.unpack('>H', 40)[0]

class Hm600Decode11(EventsResponse):
    """ Inverter generic events log """

class Hm600Decode12(EventsResponse):
    """ Inverter major events log """


# 1161-Series Inverters, 2 MPPT, 1 Phase
class Hm1200Decode02(EventsResponse):
    """ Inverter generic events log """

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
        return self.unpack('>H', 6)[0]/100
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
    def powerfactor(self):
        """ Powerfactor """
        return self.unpack('>H', 56)[0]/1000
    @property
    def temperature(self):
        """ Inverter temperature in °C """
        return self.unpack('>H', 58)[0]/10
    @property
    def event_count(self):
        """ Event counter """
        return self.unpack('>H', 60)[0]

class Hm1200Decode11(EventsResponse):
    """ Inverter generic events log """

class Hm1200Decode12(EventsResponse):
    """ Inverter major events log """
