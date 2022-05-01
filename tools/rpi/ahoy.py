"""
First attempt at providing basic 'master' ('DTU') functionality
for Hoymiles micro inverters.
Based in particular on demostrated first contact by 'of22'.
"""
import sys
import argparse
import time
import struct
import crcmod
import json
from datetime import datetime
from RF24 import RF24, RF24_PA_LOW, RF24_PA_MAX, RF24_250KBPS, RF24_CRC_DISABLED, RF24_CRC_8, RF24_CRC_16
import paho.mqtt.client
from configparser import ConfigParser
#from hoymiles import ser_to_hm_addr, ser_to_esb_addr
import hoymiles

cfg = ConfigParser()
cfg.read('ahoy.conf')
mqtt_host = cfg.get('mqtt', 'host', fallback='192.168.1.1')
mqtt_port = cfg.getint('mqtt', 'port', fallback=1883)
mqtt_user = cfg.get('mqtt', 'user', fallback='')
mqtt_password = cfg.get('mqtt', 'password', fallback='')

radio = RF24(22, 0, 1000000)
mqtt_client = paho.mqtt.client.Client()
mqtt_client.username_pw_set(mqtt_user, mqtt_password)
mqtt_client.connect(mqtt_host, mqtt_port)
mqtt_client.loop_start()

# Master Address ('DTU')
dtu_ser = cfg.get('dtu', 'serial', fallback='99978563412')  # identical to fc22's

# inverter serial numbers
inv_ser = cfg.get('inverter', 'serial', fallback='444473104619')  # my inverter

# all inverters
#...

f_crc_m = crcmod.predefined.mkPredefinedCrcFun('modbus')
f_crc8 = crcmod.mkCrcFun(0x101, initCrc=0, xorOut=0)

# time of last transmission - to calculcate response time
t_last_tx = 0

def main_loop():
    """
    Keep receiving on channel 3. Every once in a while, transmit a request
    to one of our inverters on channel 40.
    """

    global t_last_tx

    hoymiles.print_addr(inv_ser)
    hoymiles.print_addr(dtu_ser)

    ctr = 1
    last_tx_message = ''

    rx_channels = [3,6,9,11,23,40,61,75]
    rx_channel_id = 0
    rx_channel = rx_channels[rx_channel_id]
    rx_channel_ack = None
    rx_error = 0

    tx_channels = [40]
    tx_channel_id = 0
    tx_channel = tx_channels[tx_channel_id]

    radio.setChannel(rx_channel)
    radio.setRetries(10, 2)
    radio.setPALevel(RF24_PA_LOW)
    #radio.setPALevel(RF24_PA_MAX)
    radio.setDataRate(RF24_250KBPS)
    radio.openReadingPipe(1,hoymiles.ser_to_esb_addr(dtu_ser))
    radio.openWritingPipe(hoymiles.ser_to_esb_addr(inv_ser))

    while True:
        m_buf = []
        # Channel selection: Sweep receive start channel
        if not rx_channel_ack:
            rx_channel_id = ctr % len(rx_channels)
            rx_channel = rx_channels[rx_channel_id]

        tx_channel_id = tx_channel_id + 1
        if tx_channel_id >= len(tx_channels):
            tx_channel_id = 0
        tx_channel = tx_channels[tx_channel_id]

        # Transmit: Compose data
        com = hoymiles.InverterTransaction(
            request_time = datetime.now(),
            inverter_ser=inv_ser,
            request = hoymiles.compose_0x80_msg(src_ser_no=dtu_ser, dst_ser_no=inv_ser, subtype=b'\x0b')
            )
        print(com)

        # Transmit: Setup radio
        radio.stopListening()  # put radio in TX mode
        radio.setChannel(tx_channel)
        radio.setAutoAck(True)
        radio.setRetries(3, 15)
        radio.setCRCLength(RF24_CRC_16)
        radio.enableDynamicPayloads()

        # Transmit: Send payload
        t_tx_start = time.monotonic_ns()
        tx_status = radio.write(com.request)
        t_last_tx = t_tx_end = time.monotonic_ns()

        ctr = ctr + 1

        # Receive: Setup radio
        radio.setChannel(rx_channel)
        radio.setAutoAck(False)
        radio.setRetries(0, 0)
        radio.enableDynamicPayloads()
        radio.setCRCLength(RF24_CRC_16)
        radio.startListening()

        # Receive: Loop
        t_end = time.monotonic_ns()+1e9
        while time.monotonic_ns() < t_end:

            has_payload, pipe_number = radio.available_pipe()
            if has_payload:
                # Data in nRF24 buffer, read it
                rx_error = 0
                rx_channel_ack = rx_channel
                t_end = time.monotonic_ns()+2e8
                
                size = radio.getDynamicPayloadSize()
                payload = radio.read(size)
                fragment = hoymiles.InverterPacketFragment(
                        payload=payload,
                        ch_rx=rx_channel, ch_tx=tx_channel,
                        time_rx=datetime.now(),
                        latency=time.monotonic_ns()-t_last_tx
                        )
                print(fragment)
                com.frame_append(fragment)
                
            else:
                # No data in nRF rx buffer, search and wait
                # Channel lock in (not currently used)
                rx_error = rx_error + 1
                if rx_error > 0:
                    rx_channel_ack = None
                # Channel hopping
                if not rx_channel_ack:
                    rx_channel_id = rx_channel_id + 1
                    if rx_channel_id >= len(rx_channels):
                        rx_channel_id = 0
                    rx_channel = rx_channels[rx_channel_id]
                    radio.stopListening()
                    radio.setChannel(rx_channel)
                    radio.startListening()
                time.sleep(0.005)

        inv_ser_hm = hoymiles.ser_to_hm_addr(inv_ser)
        try:
            payload = com.get_payload()
        except BufferError:
            payload = None
            #print("Garbage")

        iv = None
        if payload:
            plen = len(payload)
            dt = com.time_rx.strftime("%Y-%m-%d %H:%M:%S.%f")
            iv = hoymiles.hm600_0b_response_decode(payload)

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
            print(f' inverter={com.inverter_ser}', end='')
            print(f' {iv.ac_frequency}Hz', end='')
            print(f' {iv.temperature}°C', end='')
            print()


        # output to MQTT
        if iv:
            src = com.inverter_ser
            # AC Data
            mqtt_client.publish(f'ahoy/{src}/frequency', iv.ac_frequency)
            mqtt_client.publish(f'ahoy/{src}/emeter/0/power', iv.ac_power_0)
            mqtt_client.publish(f'ahoy/{src}/emeter/0/voltage', iv.ac_voltage_0)
            mqtt_client.publish(f'ahoy/{src}/emeter/0/current', iv.ac_current_0)
            mqtt_client.publish(f'ahoy/{src}/emeter/0/total', iv.dc_energy_total_0)
            # DC Data
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/0/total', iv.dc_energy_total_0/1000)
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/0/power', iv.dc_power_0)
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/0/voltage', iv.dc_voltage_0)
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/0/current', iv.dc_current_0)
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/1/total', iv.dc_energy_total_1/1000)
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/1/power', iv.dc_power_1)
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/1/voltage', iv.dc_voltage_1)
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/1/current', iv.dc_current_1)
            # Global
            mqtt_client.publish(f'ahoy/{src}/temperature', iv.temperature)

            time.sleep(5)

        # Flush console
        print(flush=True, end='')

if __name__ == "__main__":

    if not radio.begin():
        raise RuntimeError("radio hardware is not responding")

    radio.setPALevel(RF24_PA_LOW)  # RF24_PA_MAX is default

    # radio.printDetails();  # (smaller) function that prints raw register values
    # radio.printPrettyDetails();  # (larger) function that prints human readable data

    try:
        main_loop()

    except KeyboardInterrupt:
        print(" Keyboard Interrupt detected. Exiting...")
        radio.powerDown()
        sys.exit()
