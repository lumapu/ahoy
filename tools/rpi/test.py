#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import codecs
import re
import time
from datetime import datetime
import hoymiles

logdata = """
2022-05-01 12:29:02.139673 Transmit 368223: channel=40 len=27 ack=False | 15 72 22 01 43 78 56 34 12 80 0b 00 62 6e 60 ee 00 00 00 05 00 00 00 00 7e 58 25
2022-05-01 12:29:02.184796 Received 27 bytes on channel 3 after tx 6912328ns: 95 72 22 01 43 72 22 01 43 01 00 01 01 4e 00 9d 02 0a 01 50 00 9d 02 10 00 00 91
2022-05-01 12:29:02.184796 Decoder src=72220143, dst=72220143, cmd=1,   u1=33.4V, i1=1.57A, p1=52.2W,  u2=33.6V, i2=1.57A, p2=52.8W,  uk1=1, uk2=0
2022-05-01 12:29:02.226251 Received 27 bytes on channel 75 after tx 48355619ns: 95 72 22 01 43 72 22 01 43 02 88 1f 00 00 7f 08 00 94 00 97 08 e2 13 89 03 eb ec
2022-05-01 12:29:02.226251 Decoder src=72220143, dst=72220143, cmd=2,   ac_u1=227.4V, ac_f=50.01Hz, ac_p1=100.3W, uk1=34847, uk2=0, uk3=32520, uk4=148, uk5=151
2022-05-01 12:29:02.273766 Received 23 bytes on channel 75 after tx 95876606ns: 95 72 22 01 43 72 22 01 43 83 00 01 00 2c 03 e8 00 d8 00 06 0c 35 37
2022-05-01 12:29:02.273766 Decoder src=72220143, dst=72220143, cmd=131,   ac_i1=0.44A, t=21.60C,  uk1=1, uk3=1000, uk5=6, uk6=3125
"""

def payload_from_log(line):
    values = re.match(r'(?P<datetime>\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) Received.*: (?P<data>[0-9a-z ]+)$', line)
    if values:
        payload=values.group('data')
        return hoymiles.InverterPacketFragment(
            time_rx=datetime.strptime(values.group('datetime'), '%Y-%m-%d %H:%M:%S.%f'),
            payload=bytes.fromhex(payload)
            )

with open('example-logs/example.log', 'r') as fh:
    for line in fh:
        kind = re.match(r'\d{4}-\d{2}-\d{2} \d\d:\d\d:\d\d.\d+ (?P<type>Transmit|Received)', line)
        if kind:
            if kind.group('type') == 'Transmit':
                u, data = line.split('|')
                rx_buffer = hoymiles.InverterTransaction(
                        request=bytes.fromhex(data))

            elif kind.group('type') == 'Received':
                try:
                    payload = payload_from_log(line)
                    print(payload)
                except BufferError as err:
                    print(f'Debug: {err}')
                    payload = None
                    pass
                if payload:
                    rx_buffer.frame_append(payload)
                    try:
                        #packet = rx_buffer.get_payload(72220143)
                        packet = rx_buffer.get_payload()
                    except BufferError as err:
                        print(f'Debug: {err}')
                        packet = None
                        pass

                    if packet:
                        plen = len(packet)
                        dt = rx_buffer.time_rx.strftime("%Y-%m-%d %H:%M:%S.%f")
                        iv = hoymiles.hm600_0b_response_decode(packet)

                        print(f'{dt} Decoded: {plen}', end='')
                        print(f' string1=', end='')
                        print(f' {iv.dc_voltage_0}VDC', end='')
                        print(f' {iv.dc_current_0}A', end='')
                        print(f' {iv.dc_power_0}W', end='')
                        print(f' {iv.dc_energy_total_0}Wh', end='')
                        print(f' {iv.dc_energy_daily_0}Wh/day', end='')
                        print(f' string2=', end='')
                        print(f' {iv.dc_voltage_1}VDC', end='')
                        print(f' {iv.dc_current_1}A', end='')
                        print(f' {iv.dc_power_1}W', end='')
                        print(f' {iv.dc_energy_total_1}Wh', end='')
                        print(f' {iv.dc_energy_daily_1}Wh/day', end='')
                        print(f' phase1=', end='')
                        print(f' {iv.ac_voltage_0}VAC', end='')
                        print(f' {iv.ac_current_0}A', end='')
                        print(f' {iv.ac_power_0}W', end='')
                        print(f' inverter=', end='')
                        print(f' {iv.ac_frequency}Hz', end='')
                        print(f' {iv.temperature}°C', end='')
                        print()

        print('', end='', flush=True)
