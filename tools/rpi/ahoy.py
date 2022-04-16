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

radio = RF24(22, 0, 1000000)
mqtt_client = paho.mqtt.client.Client()
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


def compose_0x80_msg(dst_ser_no=72220200, src_ser_no=72220200, ts=None):
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
    pp = b'\x0b\x00'
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

def on_receive(p, ch_rx=None, ch_tx=None):
    """
    Callback: get's invoked whenever a packet has been received.
    :param p: Payload of the received packet.
    """

    d = {}

    t_now_ns = time.monotonic_ns()
    ts = datetime.utcnow()
    ts_unixtime = ts.timestamp()
    d['ts_unixtime'] = ts_unixtime
    d['isodate'] = ts.isoformat()
    d['rawdata'] = " ".join([f"{b:02x}" for b in p])
    print(ts.isoformat(), end='Z ')

    # check crc8
    crc8 = f_crc8(p[:-1])
    d['crc8_valid'] = True if crc8==p[-1] else False

    # interpret content
    mid = p[0]
    d['mid'] = mid
    d['response_time_ns'] = t_now_ns-t_last_tx
    d['ch_rx'] = ch_rx
    d['ch_tx'] = ch_tx
    d['src'] = 'src_unkn'
    d['name'] = 'name_unkn'
 
    if mid == 0x95:
        src, dst, cmd = struct.unpack('>LLB', p[1:10])
        d['src'] = f'{src:08x}'
        d['dst'] = f'{dst:08x}'
        d['cmd'] = cmd
        print(f'MSG src={d["src"]}, dst={d["dst"]}, cmd={d["cmd"]}:')

        if cmd==1:
            d['name'] = 'dcdata'
            unknown1, u1, i1, p1, u2, i2, p2, unknown2 = struct.unpack(
                '>HHHHHHHH', p[10:26])
            d['u1_V'] = u1/10
            d['i1_A'] = i1/100
            d['p1_W'] = p1/10
            d['u2_V'] = u2/10
            d['i2_A'] = i2/100
            d['p2_W'] = p2/10
            d['p_W'] = d['p1_W']+d['p2_W']
            d['unknown1'] = unknown1
            d['unknown2'] = unknown2

        elif cmd==2:
            d['name'] = 'acdata'
            uk1, uk2, uk3, uk4, uk5, u, f, p = struct.unpack(
                '>HHHHHHHH', p[10:26])
            d['u_V'] = u/10
            d['f_Hz'] = f/100
            d['p_W'] = p/10
            d['wtot1_Wh'] = uk1
            d['wtot2_Wh'] = uk3
            d['wday1_Wh'] = uk4
            d['wday2_Wh'] = uk5
            d['uk2'] = uk2

        elif cmd==129:
            d['name'] = 'error'

        elif cmd==131:  # 0x83
            d['name'] = 'statedata'
            uk1, l, uk3, t, uk5, uk6 = struct.unpack('>HHHHHH', p[10:22])
            d['l_Pct'] = l
            d['t_C'] = t/10
            d['uk1'] = uk1
            d['uk3'] = uk3
            d['uk5'] = uk5
            d['uk6'] = uk6

        elif cmd==132:  # 0x84
            d['name'] = 'unknown0x84'
            uk1, uk2, uk3, uk4, uk5, uk6, uk7, uk8 = struct.unpack(
                '>HHHHHHHH', p[10:26])
            d['uk1'] = uk1
            d['uk2'] = uk2
            d['uk3'] = uk3
            d['uk4'] = uk4
            d['uk5'] = uk5
            d['uk6'] = uk6
            d['uk7'] = uk7
            d['uk8'] = uk8

        else:
            print(f'unknown cmd {cmd}')
    else:
        print(f'unknown frame id {p[0]}')

    # output to stdout
    if d:
        print(json.dumps(d))

    # output to MQTT
    if d:
        j = json.dumps(d)
        mqtt_client.publish(f"ahoy/{d['src']}/{d['name']}", j)
        if d['cmd']==2:
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter/0/voltage', d['u_V'])
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter/0/power', d['p_W'])
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter/0/total', d['wtot1_Wh'])
            mqtt_client.publish(f'ahoy/{d["src"]}/frequency', d['f_Hz'])
        if d['cmd']==1:
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter-dc/0/power', d['p1_W'])
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter-dc/0/voltage', d['u1_V'])
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter-dc/0/current', d['i1_A'])
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter-dc/1/power', d['p2_W'])
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter-dc/1/voltage', d['u2_V'])
            mqtt_client.publish(f'ahoy/{d["src"]}/emeter-dc/1/current', d['i2_A'])
        if d['cmd']==131:
            mqtt_client.publish(f'ahoy/{d["src"]}/temperature', d['t_C'])



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

    tx_channels = [40]
    tx_channel_id = 0
    tx_channel = tx_channels[tx_channel_id]

    while True:
        # Sweep receive start channel
        rx_channel_id = ctr % len(rx_channels)
        rx_channel = rx_channels[rx_channel_id]

        radio.setChannel(rx_channel)
        radio.enableDynamicPayloads()
        radio.setAutoAck(False)
        radio.setPALevel(RF24_PA_MAX)
        radio.setDataRate(RF24_250KBPS)
        radio.openWritingPipe(ser_to_esb_addr(inv_ser))
        radio.flush_rx()
        radio.flush_tx()
        radio.openReadingPipe(1,ser_to_esb_addr(dtu_ser))
        radio.startListening()

        tx_channel_id = tx_channel_id + 1
        if tx_channel_id >= len(tx_channels):
            tx_channel_id = 0
        tx_channel = tx_channels[tx_channel_id]

        #
        # TX
        #
        radio.stopListening()  # put radio in TX mode
        radio.setChannel(tx_channel)
        radio.openWritingPipe(ser_to_esb_addr(inv_ser))

        ts = int(time.time())
        payload = compose_0x80_msg(src_ser_no=dtu_ser, dst_ser_no=inv_ser, ts=ts)
        dt = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
        last_tx_message = f"{dt} Transmit {ctr:5d}: channel={tx_channel} len={len(payload)} | " + \
            " ".join([f"{b:02x}" for b in payload]) + f" rx_ch: {rx_channel}"
        print(last_tx_message)

        # for i in range(0,3):
        result = radio.write(payload)  # will always yield 'True' because auto-ack is disabled
        #    time.sleep(.05)

        t_last_tx = time.monotonic_ns()
        ctr = ctr + 1

        t_end = time.monotonic_ns()+5e9
        tslots = [1000]  #, 40, 50, 60, 70]  # switch channel at these ms times since transmission

        for tslot in tslots:
            t_end = t_last_tx + tslot*1e6  # ms to ns

            radio.stopListening()
            radio.setChannel(rx_channel)
            radio.startListening()
            while time.monotonic_ns() < t_end:
                has_payload, pipe_number = radio.available_pipe()
                if has_payload:
                    size = radio.getDynamicPayloadSize()
                    payload = radio.read(size)
                    # print(last_tx_message, end='')
                    last_tx_message = ''
                    dt = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
                    print(f"{dt} Received {size} bytes on channel {rx_channel} pipe {pipe_number}: " +
                          " ".join([f"{b:02x}" for b in payload]))
                    on_receive(payload, ch_rx=rx_channel, ch_tx=tx_channel)
                else:
                    pass
                    # time.sleep(0.001)

            rx_channel_id = rx_channel_id + 1
            if rx_channel_id >= len(rx_channels):
                rx_channel_id = 0
            rx_channel = rx_channels[rx_channel_id]

        print(flush=True, end='')
        # time.sleep(2)



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
