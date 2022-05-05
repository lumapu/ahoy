#!/usr/bin/python3
# -*- coding: utf-8 -*-
import struct

class StatusResponse:
    e_keys  = ['voltage','current','power','energy_total','energy_daily']

    def unpack(self, fmt, base):
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.response[base:base+size])

    @property
    def phases(self):
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
        data = {}
        data['phases'] = self.phases
        data['strings'] = self.strings
        data['temperature'] = self.temperature
        data['frequency'] = self.frequency
        return data

class UnknownResponse:
    @property
    def hex_ascii(self):
        return ' '.join([f'{b:02x}' for b in self.response])
    @property
    def dump_longs(self):
        n = len(self.response)/4
        vals = struct.unpack(f'>{int(n)}L', self.response)
        return vals

    @property
    def dump_shorts(self):
        n = len(self.response)/2
        vals = struct.unpack(f'>{int(n)}H', self.response)
        return vals

class HM600_Decode02(UnknownResponse):
    def __init__(self, response):
        self.response = response

class HM600_Decode11(UnknownResponse):
    def __init__(self, response):
        self.response = response

class HM600_Decode12(UnknownResponse):
    def __init__(self, response):
        self.response = response

class HM600_Decode0A(UnknownResponse):
    def __init__(self, response):
        self.response = response

class HM600_Decode0B(StatusResponse):
    def __init__(self, response):
        self.response = response

    @property
    def dc_voltage_0(self):
        return self.unpack('>H', 2)[0]/10
    @property
    def dc_current_0(self):
        return self.unpack('>H', 4)[0]/100
    @property
    def dc_power_0(self):
        return self.unpack('>H', 6)[0]/10
    @property
    def dc_energy_total_0(self):
        return self.unpack('>L', 14)[0]
    @property
    def dc_energy_daily_0(self):
        return self.unpack('>H', 22)[0]

    @property
    def dc_voltage_1(self):
        return self.unpack('>H', 8)[0]/10
    @property
    def dc_current_1(self):
        return self.unpack('>H', 10)[0]/100
    @property
    def dc_power_1(self):
        return self.unpack('>H', 12)[0]/10
    @property
    def dc_energy_total_1(self):
        return self.unpack('>L', 18)[0]
    @property
    def dc_energy_daily_1(self):
        return self.unpack('>H', 24)[0]

    @property
    def ac_voltage_0(self):
        return self.unpack('>H', 26)[0]/10
    @property
    def ac_current_0(self):
        return self.unpack('>H', 34)[0]/10
    @property
    def ac_power_0(self):
        return self.unpack('>H', 30)[0]/10
    @property
    def frequency(self):
        return self.unpack('>H', 28)[0]/100
    @property
    def temperature(self):
        return self.unpack('>H', 38)[0]/10

class HM600_Decode0C(HM600_Decode0B):
    def __init__(self, response):
        self.response = response


# HM-1500
class HM1500_Decode0B(StatusResponse):
    def __init__(self, response):
        self.response = response

    @property
    def dc_voltage_0(self):
        return self.unpack('>H', 2)[0]/10
    @property
    def dc_current_0(self):
        return self.unpack('>H', 4)[0]/100
    @property
    def dc_power_0(self):
        return self.unpack('>H', 6)[0]/10
    @property
    def dc_energy_total_0(self):
        return self.unpack('>L', 14)[0]
    @property
    def dc_energy_daily_0(self):
        return self.unpack('>H', 22)[0]

    @property
    def dc_voltage_1(self):
        return self.unpack('>H', 8)[0]/10
    @property
    def dc_current_1(self):
        return self.unpack('>H', 10)[0]/100
    @property
    def dc_power_1(self):
        return self.unpack('>H', 12)[0]/10
    @property
    def dc_energy_total_1(self):
        return self.unpack('>L', 18)[0]
    @property
    def dc_energy_daily_1(self):
        return self.unpack('>H', 24)[0]

    @property
    def ac_voltage_0(self):
        return self.unpack('>H', 26)[0]/10
    @property
    def ac_current_0(self):
        return self.unpack('>H', 34)[0]/10
    @property
    def ac_power_0(self):
        return self.unpack('>H', 30)[0]/10
    @property
    def frequency(self):
        return self.unpack('>H', 28)[0]/100
    @property
    def temperature(self):
        return self.unpack('>H', 38)[0]/10
