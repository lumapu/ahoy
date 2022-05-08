import struct
import crcmod
import json
import time
import re
from datetime import datetime
from RF24 import RF24, RF24_PA_LOW, RF24_PA_MAX, RF24_250KBPS, RF24_CRC_DISABLED, RF24_CRC_8, RF24_CRC_16
from .decoders import *

f_crc_m = crcmod.predefined.mkPredefinedCrcFun('modbus')
f_crc8 = crcmod.mkCrcFun(0x101, initCrc=0, xorOut=0)


HOYMILES_TRANSACTION_LOGGING=True
HOYMILES_DEBUG_LOGGING=True

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

def print_addr(a):
    print(f"ser# {a} ", end='')
    print(f" -> HM  {' '.join([f'{x:02x}' for x in ser_to_hm_addr(a)])}", end='')
    print(f" -> ESB {' '.join([f'{x:02x}' for x in ser_to_esb_addr(a)])}")

# time of last transmission - to calculcate response time
t_last_tx = 0

class ResponseDecoderFactory:
    model = None
    request = None
    response = None

    def __init__(self, response, **params):
        self.response = response

        if 'request' in params:
            self.request = params['request']
        elif hasattr(response, 'request'):
            self.request = response.request

        if 'inverter_ser' in params:
            self.inverter_ser = params['inverter_ser']
            self.model = self.inverter_model

    def unpack(self, fmt, base):
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.response[base:base+size])

    @property
    def inverter_model(self):
        if not self.inverter_ser:
            raise ValueError('Inverter serial while decoding response')

        ser_db = [
                ('HM300', r'^1121........'),
                ('HM600', r'^1141........'),
                ('HM1200', r'^1161........'),
                ]
        ser_str = str(self.inverter_ser)

        model = None
        for m, r in ser_db:
            if re.match(r, ser_str):
                model = m
                break

        if len(model):
            return model
        raise NotImplementedError('Model lookup failed for serial {ser_str}')

    @property
    def request_command(self):
        r_code = self.request[10]
        return f'{r_code:02x}'

class ResponseDecoder(ResponseDecoderFactory):
    def __init__(self, response, **params):
        ResponseDecoderFactory.__init__(self, response, **params)

    def decode(self):
        model = self.inverter_model
        command = self.request_command

        model_decoders = __import__(f'hoymiles.decoders')
        if hasattr(model_decoders, f'{model}_Decode{command.upper()}'):
            device = getattr(model_decoders, f'{model}_Decode{command.upper()}')
        else:
            if HOYMILES_DEBUG_LOGGING:
                device = getattr(model_decoders, f'DEBUG_DecodeAny')

        return device(self.response)

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

class HoymilesNRF:
    tx_channel_id = 0
    tx_channel_list = [40]
    rx_channel_id = 0
    rx_channel_list = [3,23,40,61,75]
    rx_channel_ack = False
    rx_error = 0

    def __init__(self, device):
        self.radio = device

    def transmit(self, packet):
        """
        Transmit Packet
        """

        #dst_esb_addr = b'\x01' + packet[1:5]
        #src_esb_addr = b'\x01' + packet[6:9]

        #hexify_payload(dst_esb_addr)
        #hexify_payload(src_esb_addr)

        self.radio.stopListening()  # put radio in TX mode
        self.radio.setDataRate(RF24_250KBPS)
        #self.radio.openReadingPipe(1, src_esb_addr )
        #self.radio.openWritingPipe( dst_esb_addr )
        self.radio.setChannel(self.tx_channel)
        self.radio.setAutoAck(True)
        self.radio.setRetries(3, 15)
        self.radio.setCRCLength(RF24_CRC_16)
        self.radio.enableDynamicPayloads()

        return self.radio.write(packet)

    def receive(self, timeout=None):
        """
        Receive Packets
        """

        if not timeout:
            timeout=12e8

        self.radio.setChannel(self.rx_channel)
        self.radio.setAutoAck(False)
        self.radio.setRetries(0, 0)
        self.radio.enableDynamicPayloads()
        self.radio.setCRCLength(RF24_CRC_16)
        self.radio.startListening()

        fragments = []

        # Receive: Loop
        t_end = time.monotonic_ns()+timeout
        while time.monotonic_ns() < t_end:

            has_payload, pipe_number = self.radio.available_pipe()
            if has_payload:
                # Data in nRF24 buffer, read it
                self.rx_error = 0
                self.rx_channel_ack = True
                t_end = time.monotonic_ns()+5e8

                size = self.radio.getDynamicPayloadSize()
                payload = self.radio.read(size)
                fragment = InverterPacketFragment(
                        payload=payload,
                        ch_rx=self.rx_channel, ch_tx=self.tx_channel,
                        time_rx=datetime.now()
                        )
                yield(fragment)

            else:
                # No data in nRF rx buffer, search and wait
                # Channel lock in (not currently used)
                self.rx_error = self.rx_error + 1
                if self.rx_error > 1:
                    self.rx_channel_ack = False
                # Channel hopping
                if self.next_rx_channel():
                    self.radio.stopListening()
                    self.radio.setChannel(self.rx_channel)
                    self.radio.startListening()

            time.sleep(0.005)

    def next_rx_channel(self):
        if not self.rx_channel_ack:
            self.rx_channel_id = self.rx_channel_id + 1
            if self.rx_channel_id >= len(self.rx_channel_list):
                self.rx_channel_id = 0
            return True
        return False

    @property
    def tx_channel(self):
        return self.tx_channel_list[self.tx_channel_id]

    @property
    def rx_channel(self):
        return self.rx_channel_list[self.rx_channel_id]

def frame_payload(payload):
    payload_crc = f_crc_m(payload)
    payload = payload + struct.pack('>H', payload_crc)

    return payload

def compose_esb_fragment(fragment, seq=b'\80', src=99999999, dst=1, **params):
    if len(fragment) > 17:
        raise ValueError(f'ESB fragment exeeds mtu ({mtu}): Fragment size {len(fragment)} bytes')

    p = b''
    p = p + b'\x15'
    p = p + ser_to_hm_addr(dst)
    p = p + ser_to_hm_addr(src)
    p = p + seq

    p = p + fragment

    crc8 = f_crc8(p)
    p = p + struct.pack('B', crc8)

    return p

def compose_esb_packet(packet, mtu=17, **params):
    for i in range(0, len(packet), mtu):
        fragment = compose_esb_fragment(packet[i:i+mtu], **params)
        yield(fragment)

def compose_set_time_payload(timestamp=None):
    if not timestamp:
        timestamp = int(time.time())

    payload = b'\x0b\x00'
    payload = payload + struct.pack('>L', timestamp)  # big-endian: msb at low address
    payload = payload + b'\x00\x00\x00\x05\x00\x00\x00\x00'

    return frame_payload(payload)

def compose_02_payload(timestamp=None):
    payload = b'\x02'
    if timestamp:
        payload = payload + b'\x00'
        payload = payload + struct.pack('>L', timestamp)  # big-endian: msb at low address
        payload = payload + b'\x00\x00\x00\x05\x00\x00\x00\x00'

    return frame_payload(payload)

def compose_11_payload():
    payload = b'\x11'

    return frame_payload(payload)


class InverterTransaction:
    tx_queue = []
    scratch = []
    inverter_ser = None
    inverter_addr = None
    dtu_ser = None
    req_type = None

    radio = None

    def __init__(self,
            request_time=None,
            inverter_ser=None,
            dtu_ser=None,
            radio=None,
            **params):

        if radio:
            self.radio = radio

        if not request_time:
            request_time=datetime.now()

        self.scratch = []
        if 'scratch' in params:
            self.scratch = params['scratch']

        self.inverter_ser = inverter_ser
        if inverter_ser:
            self.inverter_addr = ser_to_hm_addr(inverter_ser)

        self.dtu_ser = dtu_ser
        if dtu_ser:
            self.dtu_addr = ser_to_hm_addr(dtu_ser)

        self.request = None
        if 'request' in params:
            self.request = params['request']
            self.queue_tx(self.request)
            self.inverter_addr, self.dtu_addr, seq, self.req_type = struct.unpack('>LLBB', params['request'][1:11])
        self.request_time = request_time

    def rxtx(self):
        """
        Transmit next packet from tx_queue if available
        and wait for responses
        """
        if not self.radio:
            return False

        if not len(self.tx_queue):
            return False

        packet = self.tx_queue.pop(0)

        if HOYMILES_TRANSACTION_LOGGING:
            dt = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
            print(f'{dt} Transmit {len(packet)} | {hexify_payload(packet)}')
        
        self.radio.transmit(packet)

        wait = False
        try:
            for response in self.radio.receive():
                if HOYMILES_TRANSACTION_LOGGING:
                    print(response)
        
                self.frame_append(response)
                wait = True
        except TimeoutError:
            pass

        return wait

    def frame_append(self, payload_frame):
        """
        Append received raw frame to local scratch buffer
        """
        self.scratch.append(payload_frame)

    def queue_tx(self, frame):
        """
        Enqueue packet for transmission if radio is available
        """
        if not self.radio:
            return False

        self.tx_queue.append(frame)

        return True

    def get_payload(self, src=None):
        """
        Reconstruct Hoymiles payload from scratch
        """

        if not src:
            src = self.inverter_addr

        # Collect all frames from source_address src
        frames = [frame for frame in self.scratch if frame.src == src]

        tr_len = 0
        # Find end frame and extract message frame count
        try:
            end_frame = next(frame for frame in frames if frame.seq > 0x80)
            self.time_rx = end_frame.time_rx
            tr_len = end_frame.seq - 0x80
        except StopIteration:
            raise BufferError(f'Missing packet: Last packet {len(self.scratch)}')

        # Rebuild payload from unordered frames
        payload = b''
        for frame_id in range(1, tr_len):
            try:
                data_frame = next(item for item in frames if item.seq == frame_id)
                payload = payload + data_frame.data
            except StopIteration:
                self.__retransmit_frame(frame_id)
                raise BufferError(f'Frame {frame_id} missing: Request Retransmit')

        payload = payload + end_frame.data

        # check crc
        pcrc = struct.unpack('>H', payload[-2:])[0]
        if f_crc_m(payload[:-2]) != pcrc:
            raise ValueError('Payload failed CRC check.')

        return payload

    def __retransmit_frame(self, frame_id):
        """
        Build and queue retransmit request
        """
        packet = compose_esb_fragment(b'',
                seq=int(0x80 + frame_id).to_bytes(1, 'big'),
                src=self.dtu_ser,
                dst=self.inverter_ser)

        return self.queue_tx(packet)

    def __str__(self):
        dt = self.request_time.strftime("%Y-%m-%d %H:%M:%S.%f")
        size = len(self.request)
        return f'{dt} Transmit | {hexify_payload(self.request)}'

def hexify_payload(byte_var):
    return ' '.join([f"{b:02x}" for b in byte_var])
