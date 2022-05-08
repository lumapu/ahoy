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
        res = self.response
        n = len(res)/4

        vals = None
        if n % 4 == 0:
            vals = struct.unpack(f'>{int(n)}L', res)

        return vals

    @property
    def dump_longs_pad1(self):
        res = self.response[1:]
        n = len(res)/4

        vals = None
        if n % 4 == 0:
            vals = struct.unpack(f'>{int(n)}L', res)

        return vals

    @property
    def dump_shorts(self):
        n = len(self.response)/2

        vals = None
        if n % 2 == 0:
            vals = struct.unpack(f'>{int(n)}H', self.response)
        return vals

    @property
    def dump_shorts_pad1(self):
        res = self.response[1:]
        n = len(res)/2

        vals = None
        if n % 2 == 0:
            vals = struct.unpack(f'>{int(n)}H', res)
        return vals

class DEBUG_DecodeAny(UnknownResponse):
    def __init__(self, response):
        self.response = response

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
        return self.unpack('>H', 8)[0]/10
    @property
    def dc_energy_total_0(self):
        return self.unpack('>L', 12)[0]
    @property
    def dc_energy_daily_0(self):
        return self.unpack('>H', 20)[0]

    @property
    def dc_voltage_1(self):
        return self.unpack('>H', 2)[0]/10
    @property
    def dc_current_1(self):
        return self.unpack('>H', 4)[0]/100
    @property
    def dc_power_1(self):
        return self.unpack('>H', 10)[0]/10
    @property
    def dc_energy_total_1(self):
        return self.unpack('>L', 16)[0]
    @property
    def dc_energy_daily_1(self):
        return self.unpack('>H', 22)[0]

    @property
    def dc_voltage_2(self):
        return self.unpack('>H', 24)[0]/10
    @property
    def dc_current_2(self):
        return self.unpack('>H', 26)[0]/100
    @property
    def dc_power_2(self):
        return self.unpack('>H', 30)[0]/10
    @property
    def dc_energy_total_2(self):
        return self.unpack('>L', 34)[0]
    @property
    def dc_energy_daily_2(self):
        return self.unpack('>H', 42)[0]

    @property
    def dc_voltage_3(self):
        return self.unpack('>H', 24)[0]/10
    @property
    def dc_current_3(self):
        return self.unpack('>H', 28)[0]/100
    @property
    def dc_power_3(self):
        return self.unpack('>H', 32)[0]/10
    @property
    def dc_energy_total_3(self):
        return self.unpack('>L', 38)[0]
    @property
    def dc_energy_daily_3(self):
        return self.unpack('>H', 44)[0]

    @property
    def ac_voltage_0(self):
        return self.unpack('>H', 46)[0]/10
    @property
    def ac_current_0(self):
        return self.unpack('>H', 54)[0]/100
    @property
    def ac_power_0(self):
        return self.unpack('>H', 50)[0]/10
    @property
    def frequency(self):
        return self.unpack('>H', 48)[0]/100
    @property
    def temperature(self):
        return self.unpack('>H', 58)[0]/10
