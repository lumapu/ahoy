import struct
import crcmod
import json
import time
from datetime import datetime
from RF24 import RF24, RF24_PA_LOW, RF24_PA_MAX, RF24_250KBPS, RF24_CRC_DISABLED, RF24_CRC_8, RF24_CRC_16

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
        ts = int(time.time())

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

class hm600_02_response_decode:
    """ TBD """
    def __init__(self, response):
        self.response = response

class hm600_11_response_decode:
    """ TBD """
    def __init__(self, response):
        self.response = response

class hm600_0b_response_decode:
    def __init__(self, response):
        self.response = response

    def unpack(self, fmt, base):
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.response[base:base+size])

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
    def ac_frequency(self):
        return self.unpack('>H', 28)[0]/100
    @property
    def temperature(self):
        return self.unpack('>H', 38)[0]/10

class InverterPacketFragment:
    def __init__(self, time_rx=None, payload=None, ch_rx=None, ch_tx=None, **params):
        """
        Callback: get's invoked whenever a Nordic ESB packet has been received.
        :param p: Payload of the received packet.
        """

        if not time_rx:
            time_rx = datetime.now()
        self.time_rx = time_rx

        self.frame = payload

        # check crc8
        if f_crc8(payload[:-1]) != payload[-1]:
            raise BufferError('Frame kaputt')

        self.ch_rx = ch_rx
        self.ch_tx = ch_tx

    @property
    def mid(self):
        """
        Transaction counter
        """
        return self.frame[0]
    @property
    def src(self):
        """
        Sender dddress
        """
        src = struct.unpack('>L', self.frame[1:5])
        return src[0]
    @property
    def dst(self):
        """
        Receiver address
        """
        dst = struct.unpack('>L', self.frame[5:8])
        return dst[0]
    @property
    def seq(self):
        """
        Packet sequence
        """
        result = struct.unpack('>B', self.frame[9:10])
        return result[0]
    @property
    def data(self):
        """
        Packet without protocol framing
        """
        return self.frame[10:-1]

    def __str__(self):
        dt = self.time_rx.strftime("%Y-%m-%d %H:%M:%S.%f")
        size = len(self.frame)
        channel = f' channel {self.ch_rx}' if self.ch_rx else ''
        raw = " ".join([f"{b:02x}" for b in self.frame])
        return f"{dt} Received {size} bytes{channel}: {raw}"

class InverterTransaction:
    def __init__(self,
            request_time=datetime.now(),
            inverter_ser=None,
            dtu_ser=None,
            **params):
        self.scratch = []
        if 'scratch' in params:
            self.scratch = params['scratch']

        self.inverter_ser = inverter_ser
        if inverter_ser:
            self.peer_src = ser_to_hm_addr(inverter_ser)

        self.dtu_ser = dtu_ser
        if dtu_ser:
            self.dtu_dst = ser_to_hm_addr(dtu_ser)

        self.peer_src, self.peer_dst, self.req_type = (None,None,None)

        self.request = None
        if 'request' in params:
            self.request = params['request']
            self.peer_src, self.peer_dst, skip, self.req_type = struct.unpack('>LLBB', params['request'][1:11])
        self.request_time = request_time

    def frame_append(self, payload_frame):
        self.scratch.append(payload_frame)

    def get_payload(self, src=None):
        """
        Reconstruct Hoymiles payload from scratch
        """

        if not src:
            src = self.peer_src

        # Collect all frames from source_address src
        frames = [frame for frame in self.scratch if frame.src == src]

        tr_len = 0
        # Find end frame and extract message frame count
        try:
            end_frame = next(frame for frame in frames if frame.seq > 0x80)
            self.time_rx = end_frame.time_rx
            tr_len = end_frame.seq - 0x80
        except StopIteration:
            raise BufferError('Missing packet: Last packet')

        # Rebuild payload from unordered frames
        payload = b''
        seq_missing = []
        for i in range(1, tr_len):
            try:
                data_frame = next(item for item in frames if item.seq == i)
                payload = payload + data_frame.data
            except StopIteration:
                seq_missing.append(i)
                pass

        payload = payload + end_frame.data

        # check crc
        pcrc = struct.unpack('>H', payload[-2:])[0]
        if f_crc_m(payload[:-2]) != pcrc:
            raise BufferError('Payload failed CRC check.')

        return payload

    def __str__(self):
        dt = self.request_time.strftime("%Y-%m-%d %H:%M:%S.%f")
        size = len(self.request)
        raw = " ".join([f"{b:02x}" for b in self.request])
        return f'{dt} Transmit | {raw}'
