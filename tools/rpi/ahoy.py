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
from RF24 import RF24, RF24_PA_LOW, RF24_PA_MAX, RF24_250KBPS
import paho.mqtt.client
from configparser import ConfigParser

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


def ser_to_hm_addr(s):
    """
    Calculate the 4 bytes that the HM devices use in their internal messages to 
    address each other.
    """
    bcd = int(str(s)[-8:], base=16)
    return struct.pack('>L', bcd)


def ser_to_esb_addr(s):
    """
    Convert a Hoymiles inverter/DTU serial number into its
    corresponding NRF24 'enhanced shockburst' address byte sequence (5 bytes).

    The NRF library expects these in LSB to MSB order, even though the transceiver
    itself will then output them in MSB-to-LSB order over the air.
    
    The inverters use a BCD representation of the last 8
    digits of their serial number, in reverse byte order, 
    followed by \x01.
    """
    air_order = ser_to_hm_addr(s)[::-1] + b'\x01'
    return air_order[::-1]


def compose_0x80_msg(dst_ser_no=72220200, src_ser_no=72220200, ts=None, subtype=b'\x0b'):
    """
    Create a valid 0x80 request with the given parameters, and containing the 
    current system time.
    """

    if not ts:
        ts = 0x623C8ECF  # identical to fc22's for testing  # doc: 1644758171

    # "framing"
    p = b''
    p = p + b'\x15'
    p = p + ser_to_hm_addr(dst_ser_no)
    p = p + ser_to_hm_addr(src_ser_no)
    p = p + b'\x80'

    # encapsulated payload
    pp = subtype + b'\x00'
    pp = pp + struct.pack('>L', ts)  # big-endian: msb at low address
    #pp = pp + b'\x00' * 8    # of22 adds a \x05 at position 19

    pp = pp + b'\x00\x00\x00\x05\x00\x00\x00\x00'

    # CRC_M
    crc_m = f_crc_m(pp)

    p = p + pp
    p = p + struct.pack('>H', crc_m)

    crc8 = f_crc8(p)
    p = p + struct.pack('B', crc8)   
    return p


def print_addr(a):
    print(f"ser# {a} ", end='')
    print(f" -> HM  {' '.join([f'{x:02x}' for x in ser_to_hm_addr(a)])}", end='')
    print(f" -> ESB {' '.join([f'{x:02x}' for x in ser_to_esb_addr(a)])}")

# time of last transmission - to calculcate response time
t_last_tx = 0


def on_receive(p=None, ctr=None, ch_rx=None, ch_tx=None, time_rx=datetime.now(), latency=None):
    """
    Callback: get's invoked whenever a Nordic ESB packet has been received.
    :param p: Payload of the received packet.
    """

    d = {}

    t_now_ns = time.monotonic_ns()
    ts = datetime.utcnow()
    ts_unixtime = ts.timestamp()
    size = len(p)
    d['ts_unixtime'] = ts_unixtime
    d['isodate'] = ts.isoformat()
    d['rawdata'] = " ".join([f"{b:02x}" for b in p])
    d['trans_id'] = ctr

    dt = time_rx.strftime("%Y-%m-%d %H:%M:%S.%f")
    print(f"{dt} Received {size} bytes on channel {ch_rx} after tx {latency}ns: " +
        " ".join([f"{b:02x}" for b in p]))

    # check crc8
    crc8 = f_crc8(p[:-1])
    d['crc8_valid'] = True if crc8==p[-1] else False

    # interpret content
    mid = p[0]
    d['mid'] = mid
    name = 'unknowndata'
    d['response_time_ns'] = t_now_ns-t_last_tx
    d['ch_rx'] = ch_rx
    d['ch_tx'] = ch_tx
 
    if mid == 0x95:
        decode_hoymiles_hm600(d, p, time_rx=time_rx)
    else:
        print(f'unknown frame id {p[0]}')


def decode_hoymiles_hm600(d, p, time_rx=datetime.now()):
    """
    Decode payload from Hoymiles HM-600
    :param d: Pre parsed data from on_receive
    :param p: raw payload byte array
    :param time_rx: datetime object when packet was received
    """
    src, dst, cmd = struct.unpack('>LLB', p[1:10])
    src_s = f'{src:08x}'
    dst_s = f'{dst:08x}'
    d['src'] = src_s
    d['dst'] = dst_s
    d['cmd'] = cmd
    dt = time_rx.strftime("%Y-%m-%d %H:%M:%S.%f")
    print(f'{dt} Decoder src={src_s}, dst={dst_s}, cmd={cmd},  ', end=' ')

    if cmd==1: # 0x01
        """
        On HM600 Response to
            0x80 0x0b
            0x80 0x0c
            0x80 0x0d
            0x80 0x0f
            0x80 0x03 (garbled data)
        """
        name = 'dcdata'
        uk1, u1, i1, p1, u2, i2, p2, uk2 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        print(f'u1={u1/10}V, i1={i1/100}A, p1={p1/10}W,  ', end='')
        print(f'u2={u2/10}V, i2={i2/100}A, p2={p2/10}W,  ', end='')
        print(f'uk1={uk1}, uk2={uk2}')
        d['dc'] = {0: {}, 1: {}}
        d['dc'][0]['voltage'] = u1/10
        d['dc'][0]['current'] = i1/100
        d['dc'][0]['power'] = p1/10
        d['dc'][1]['voltage'] = u2/10
        d['dc'][1]['current'] = i2/100
        d['dc'][1]['power'] = p2/10
        d['uk1'] = uk1
        d['uk2'] = uk2

    elif cmd==2: # 0x02
        """
        On HM600 Response to
            0x80 0x0b
            0x80 0x0c
            0x80 0x0d
            0x80 0x0f
            0x80 0x03 (garbled data)
        """
        name = 'acdata'
        uk1, uk2, uk3, uk4, uk5, ac_u1, f, ac_p1 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        print(f'ac_u1={ac_u1/10:.1f}V, ac_f={f/100:.2f}Hz, ac_p1={ac_p1/10:.1f}W, ', end='')
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}')
        d['ac'] = {0: {}}
        d['ac'][0]['voltage'] = ac_u1/10
        d['frequency'] = f/100
        d['ac'][0]['power'] = ac_p1/10
        d['wtot1_Wh'] = uk1
        d['wtot2_Wh'] = uk3
        d['wday1_Wh'] = uk4
        d['wday2_Wh'] = uk5
        d['uk2'] = uk2

    elif cmd==3:  # 0x03
        """
        On HM600 Response to
            0x80 0x03 (garbled data)
        """
        uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        name = 'error'
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}, ', end='')
        print(f'uk7={uk7}, ', end='')
        print(f'uk8={uk8}')

    elif cmd==4:  # 0x04
        """
        On HM600 Response to
            0x80 0x03 (garbled data)
        """
        uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        name = 'error'
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}, ', end='')
        print(f'uk7={uk7}, ', end='')
        print(f'uk8={uk8}')

    elif cmd==5:  # 0x05
        """
        On HM600 Response to
            0x80 0x03 (garbled data)
        """
        uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        name = 'error'
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}, ', end='')
        print(f'uk7={uk7}, ', end='')
        print(f'uk8={uk8}')

    elif cmd==6:  # 0x06
        """
        On HM600 Response to
            0x80 0x03 (garbled data)
        """
        uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        name = 'error'
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}, ', end='')
        print(f'uk7={uk7}, ', end='')
        print(f'uk8={uk8}')

    elif cmd==7:  # 0x07
        """
        On HM600 Response to
            0x80 0x03 (garbled data)
        """
        uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        name = 'error'
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}, ', end='')
        print(f'uk7={uk7}, ', end='')
        print(f'uk8={uk8}')

    elif cmd==129 and len(p) == 17:  # 0x81
        """
        On HM600 Response to
            0x80 0x0a
        """
        uk1, uk2, uk3 = struct.unpack(
            '>HHH', p[10:16])
        name = 'error'
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ')

    elif cmd==129:  # 0x81
        """
        On HM600 Response to
            0x80 0x02
            0x80 0x11
        """
        uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        name = 'error'
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}, ', end='')
        print(f'uk7={uk7}, ', end='')
        print(f'uk8={uk8}')

    elif cmd==131:  # 0x83
        """
        On HM600 Response to
            0x80 0x0b
            0x80 0x0c
            0x80 0x0d
            0x80 0x0f
        """
        name = 'statedata'
        uk1, ac_i1, uk3, t, uk5, uk6 = struct.unpack('>HHHHHH', p[10:22])
        print(f'ac_i1={ac_i1/100}A, t={t/10:.2f}C,  ', end='')
        print(f'uk1={uk1}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}')
        d['ac'] = {0: {}}
        d['ac'][0]['current'] = ac_i1/100
        d['temperature'] = t/10
        d['uk1'] = uk1
        d['uk3'] = uk3
        d['uk5'] = uk5
        d['uk6'] = uk6

    elif cmd==132:  # 0x84
        name = 'unknown0x84'
        uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
            '>HHHHHHHH', p[10:26])
        print(f'uk1={uk1}, ', end='')
        print(f'uk2={uk2}, ', end='')
        print(f'uk3={uk3}, ', end='')
        print(f'uk4={uk4}, ', end='')
        print(f'uk5={uk5}, ', end='')
        print(f'uk6={uk6}, ', end='')
        print(f'uk7={uk7}, ', end='')
        print(f'uk8={uk8}')

    else:
        print(f'unknown cmd {cmd}')

    # output to MQTT
    if d:
        j = json.dumps(d)
        mqtt_client.publish(f'ahoy/{src}/debug', j)
        if d['cmd']==2:
            mqtt_client.publish(f'ahoy/{src}/emeter/0/voltage', d['ac'][0]['voltage'])
            mqtt_client.publish(f'ahoy/{src}/emeter/0/power', d['ac'][0]['power'])
            mqtt_client.publish(f'ahoy/{src}/emeter/0/total', d['wtot1_Wh'])
            mqtt_client.publish(f'ahoy/{src}/frequency', d['frequency'])
        if d['cmd']==1:
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/0/power', d['dc'][0]['power'])
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/0/voltage', d['dc'][0]['voltage'])
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/0/current', d['dc'][0]['current'])
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/1/power', d['dc'][1]['power'])
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/1/voltage', d['dc'][1]['voltage'])
            mqtt_client.publish(f'ahoy/{src}/emeter-dc/1/current', d['dc'][1]['current'])
        if d['cmd']==131:
            mqtt_client.publish(f'ahoy/{src}/temperature', d['temperature'])
            mqtt_client.publish(f'ahoy/{src}/emeter/0/current', d['ac'][0]['current'])


def main_loop():
    """
    Keep receiving on channel 3. Every once in a while, transmit a request
    to one of our inverters on channel 40.
    """

    global t_last_tx

    print_addr(inv_ser)
    print_addr(dtu_ser)

    ctr = 1
    last_tx_message = ''

    ts = int(time.time())  # see what happens if we always send one and the same (constant) time!
    
    rx_channels = [3,23,61,75]
    rx_channel_id = 0
    rx_channel = rx_channels[rx_channel_id]
    rx_channel_ack = None
    rx_error = 0

    tx_channels = [40]
    tx_channel_id = 0
    tx_channel = tx_channels[tx_channel_id]

    radio.setChannel(rx_channel)
    radio.enableDynamicPayloads()
    radio.setAutoAck(True)
    radio.setRetries(15, 2)
    radio.setPALevel(RF24_PA_LOW)
    #radio.setPALevel(RF24_PA_MAX)
    radio.setDataRate(RF24_250KBPS)
    radio.openReadingPipe(1,ser_to_esb_addr(dtu_ser))
    radio.openWritingPipe(ser_to_esb_addr(inv_ser))

    while True:
        radio.flush_rx()
        radio.flush_tx()
        m_buf = []
        # Sweep receive start channel
        if not rx_channel_ack:
            rx_channel_id = ctr % len(rx_channels)
            rx_channel = rx_channels[rx_channel_id]

        tx_channel_id = tx_channel_id + 1
        if tx_channel_id >= len(tx_channels):
            tx_channel_id = 0
        tx_channel = tx_channels[tx_channel_id]

        # Transmit
        ts = int(time.time())
        payload = compose_0x80_msg(src_ser_no=dtu_ser, dst_ser_no=inv_ser, ts=ts, subtype=b'\x0b')
        dt = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")

        radio.stopListening()  # put radio in TX mode
        radio.setChannel(tx_channel)
        t_tx_start = time.monotonic_ns()
        tx_status = radio.write(payload)  # will always yield 'True' because auto-ack is disabled
        t_last_tx = t_tx_end = time.monotonic_ns()
        radio.setChannel(rx_channel)
        radio.startListening()

        last_tx_message = f"{dt} Transmit {ctr:5d}: channel={tx_channel} len={len(payload)} ack={tx_status} | " + \
            " ".join([f"{b:02x}" for b in payload]) + "\n"
        ctr = ctr + 1

        # Receive loop
        t_end = time.monotonic_ns()+1e9
        while time.monotonic_ns() < t_end:
            has_payload, pipe_number = radio.available_pipe()
            if has_payload:
                # Data in nRF24 buffer, read it
                rx_error = 0
                rx_channel_ack = rx_channel
                t_end = time.monotonic_ns()+6e7
                
                size = radio.getDynamicPayloadSize()
                payload = radio.read(size)
                m_buf.append( {
                    'p': payload,
                    'ch_rx': rx_channel, 'ch_tx': tx_channel,
                    'time_rx': datetime.now(), 'latency': time.monotonic_ns()-t_last_tx} )

                # Only print last transmittet message if we got any response
                print(last_tx_message, end='')
                last_tx_message = ''
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

        # Process receive buffer outside time critical receive loop
        for param in m_buf:
            on_receive(**param)

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
