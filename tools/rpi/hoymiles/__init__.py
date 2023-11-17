#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Hoymiles micro-inverters python shared code
"""

import struct
import time
import re
from datetime import datetime
import logging
import crcmod
from .decoders import *
from os import environ

try:
  # OSI Layer 2 driver for nRF24L01 on Arduino & Raspberry Pi/Linux Devices
  # https://github.com/nRF24/RF24.git
  from RF24 import RF24, RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX, RF24_250KBPS, RF24_CRC_DISABLED, RF24_CRC_8, RF24_CRC_16
  if environ.get('TERM') is not None:
    print('Using python Module: RF24')
except ModuleNotFoundError as e:
  if environ.get('TERM') is not None:
    print(f'{e} - try to use module: RF24')
  try:
    # Repo for pyRF24 package
    # https://github.com/nRF24/pyRF24.git
    from pyrf24 import RF24, RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX, RF24_250KBPS, RF24_CRC_DISABLED, RF24_CRC_8, RF24_CRC_16
    if environ.get('TERM') is not None:
      print(f'{e} - Using python Module: pyrf24')
  except ModuleNotFoundError as e:
    if environ.get('TERM') is not None:
      print(f'{e} - exit')
    exit()

f_crc_m = crcmod.predefined.mkPredefinedCrcFun('modbus')
f_crc8 = crcmod.mkCrcFun(0x101, initCrc=0, xorOut=0)

HOYMILES_TRANSACTION_LOGGING=False
HOYMILES_DEBUG_LOGGING=False

def ser_to_hm_addr(inverter_ser):
    """
    Calculate the 4 bytes that the HM devices use in their internal messages to
    address each other.

    :param str inverter_ser: inverter serial
    :return: inverter address
    :rtype: bytes
    """
    bcd = int(str(inverter_ser)[-8:], base=16)
    return struct.pack('>L', bcd)

def ser_to_esb_addr(inverter_ser):
    """
    Convert a Hoymiles inverter/DTU serial number into its
    corresponding NRF24 'enhanced shockburst' address byte sequence (5 bytes).

    The NRF library expects these in LSB to MSB order, even though the transceiver
    itself will then output them in MSB-to-LSB order over the air.

    The inverters use a BCD representation of the last 8
    digits of their serial number, in reverse byte order,
    followed by \x01.

    :param str inverter_ser: inverter serial
    :return: ESB inverter address
    :rtype: bytes
    """
    air_order = ser_to_hm_addr(inverter_ser)[::-1] + b'\x01'
    return air_order[::-1]

class ResponseDecoderFactory:
    """
    Prepare payload decoder

    :param bytes response: ESB response frame to decode
    :param request: ESB request frame
    :type request: bytes
    :param inverter_ser: inverter serial
    :type inverter_ser: str
    :param time_rx: idatetime when payload was received
    :type time_rx: datetime
    """
    model = None
    request = None
    response = None
    time_rx = None

    def __init__(self, response, **params):
        self.response = response

        self.time_rx = params.get('time_rx', datetime.now())

        if 'request' in params:
            self.request = params['request']
        elif hasattr(response, 'request'):
            self.request = response.request

        if 'inverter_ser' in params:
            self.inverter_ser = params['inverter_ser']
            self.model = self.inverter_model

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
    def inverter_model(self):
        """
        Find decoder for inverter model

        :return: suitable decoder model string
        :rtype: str
        :raises ValueError: on invalid inverter serial
        :raises NotImplementedError: if inverter model can not be determined
        """
        if not self.inverter_ser:
            raise ValueError('Inverter serial while decoding response')

        ser_db = [
                ('Hm300', r'^1121........'),
                ('Hm600', r'^1141........'),
                ('Hm1200', r'^1161........'),
                ]
        ser_str = str(self.inverter_ser)

        model = None
        for s_model, r_match in ser_db:
            if re.match(r_match, ser_str):
                model = s_model
                break

        if len(model):
            return model
        raise NotImplementedError('Model lookup failed for serial {ser_str}')

    @property
    def request_command(self):
        """
        Return requested command identifier byte

        :return: hexlified command byte string
        :rtype: str
        """
        r_code = self.request[10]
        return f'{r_code:02x}'

class ResponseDecoder(ResponseDecoderFactory):
    """
    Base response

    :param bytes response: ESB frame response
    """
    def __init__(self, response, **params):
        """Initialize ResponseDecoder"""
        ResponseDecoderFactory.__init__(self, response, **params)
        self.inv_name=params.get('inverter_name', None)
        self.dtu_ser=params.get('dtu_ser', None)
        self.strings=params.get('strings', None)

    def decode(self):
        """
        Decode Payload

        :return: payload decoder instance
        :rtype: object
        """
        model = self.inverter_model
        command = self.request_command

        if HOYMILES_DEBUG_LOGGING:
            if   command.upper() == '00':
                model_desc = "Inverter Dev Inform Simple"
            elif command.upper() == '01':
                model_desc = "Firmware version / date"
            elif command.upper() == '02':
                model_desc = "Inverter generic events log"
            elif command.upper() == '03':                         ## HardWareConfig
                model_desc = "Hardware configuration"
            elif command.upper() == '04':                         ## SimpleCalibrationPara
                model_desc = "Simple Calibration Parameter"
            elif command.upper() == '05':                         ## SystemConfigPara
                model_desc = "Inverter generic SystemConfigPara"
            elif command.upper() == '0B':                         ## 11 - RealTimeRunData_Debug
                model_desc = "mirco-inverters status data"
            elif command.upper() == '0C':                         ## 12 - RealTimeRunData_Reality
                model_desc = "mirco-inverters status data"
            elif command.upper() == '0D':                         ## 13 - RealTimeRunData_A_Phase
                model_desc = "Real-Time Run Data A Phase "
            elif command.upper() == '0E':                         ## 14 - RealTimeRunData_B_Phase
                model_desc = "Real-Time Run Data B Phase "
            elif command.upper() == '0F':                         ## 15 - RealTimeRunData_C_Phase
                model_desc = "Real-Time Run Data C Phase "
            elif command.upper() == '11':                         ## 17 - AlarmData
                model_desc = "Inverter generic events log"
            elif command.upper() == '12':                         ## 18 - AlarmUpdate
                model_desc = "Inverter major events log"
            elif command.upper() == '13':                         ## 19 - RecordData
                model_desc = "Record Data"
            elif command.upper() == '14':                         ## 20 - InternalData
                model_desc = "Internal Data"
            elif command.upper() == '15':                         ## 21 - GetLossRate
                model_desc = "Get Loss Rate"
            elif command.upper() == '1E':                         ## 30 - GetSelfCheckState
                model_desc = "Get Self Check State"
            elif command.upper() == 'FF':                         ## 255 - InitDataState
                model_desc = "Initi Data State"

            else:
                model_desc = "event not configured - check ahoy script"
            logging.info(f'model_decoder: {model}Decode{command.upper()} - {model_desc}')

        model_decoders = __import__('hoymiles.decoders')
        if hasattr(model_decoders, f'{model}Decode{command.upper()}'):
            device = getattr(model_decoders, f'{model}Decode{command.upper()}')
        else:
            device = getattr(model_decoders, 'DebugDecodeAny')

        return device(self.response,
                time_rx=self.time_rx,
                inverter_ser=self.inverter_ser,
                inverter_name=self.inv_name,
                dtu_ser=self.dtu_ser,
                strings=self.strings
                )

class InverterPacketFragment:
    """ESB Frame"""
    def __init__(self, time_rx=None, payload=None, ch_rx=None, ch_tx=None, **params):
        """
        Callback: get's invoked whenever a Nordic ESB packet has been received.

        :param time_rx: datetime when frame was received
        :type time_rx: datetime
        :param payload: payload bytes
        :type payload: bytes
        :param ch_rx: channel where packet was received
        :type ch_rx: int
        :param ch_tx: channel where request was sent
        :type ch_tx: int

        :raises BufferError: when data gets lost on SPI bus
        """

        if not time_rx:
            time_rx = datetime.now()
        self.time_rx = time_rx

        self.frame = payload

        # check crc8
        if f_crc8(payload[:-1]) != payload[-1]:
            raise BufferError('Frame corrupted - crc8 check failed')

        self.ch_rx = ch_rx
        self.ch_tx = ch_tx

    @property
    def mid(self):
        """Transaction counter"""
        return self.frame[0]

    @property
    def src(self):
        """
        Sender adddress

        :return: sender address
        :rtype: int
        """
        src = struct.unpack('>L', self.frame[1:5])
        return src[0]
    @property
    def dst(self):
        """
        Receiver adddress

        :return: receiver address
        :rtype: int
        """
        dst = struct.unpack('>L', self.frame[5:8])
        return dst[0]
    @property
    def seq(self):
        """
        Framne sequence number

        :return: sequence number
        :rtype: int
        """
        result = struct.unpack('>B', self.frame[9:10])
        return result[0]
    @property
    def data(self):
        """
        Data without protocol framing

        :return: payload chunk
        :rtype: bytes
        """
        return self.frame[10:-1]

    def __str__(self):
        """
        Represent received ESB frame

        :return: log line received frame
        :rtype: str
        """
        size = len(self.frame)
        channel = f' channel {self.ch_rx}' if self.ch_rx else ''
        return f"Received {size} bytes{channel}: {hexify_payload(self.frame)}"

class HoymilesNRF:
    """Hoymiles NRF24 Interface"""
    tx_channel_id = 2
    tx_channel_list = [3,23,40,61,75]
    rx_channel_id = 0
    rx_channel_list = [3,23,40,61,75]
    rx_channel_ack = False
    rx_error = 0
    txpower = 'max'

    def __init__(self, **radio_config):
        """
        Claim radio device

        :param NRF24 device: instance of NRF24
        """
        radio = RF24(
                radio_config.get('ce_pin', 22),
                radio_config.get('cs_pin', 0),
                radio_config.get('spispeed', 1000000))

        if not radio.begin():
            raise RuntimeError('Can\'t open radio')
        
        if not radio.isChipConnected():
            logging.warning("could not connect to NRF24 radio")

        self.txpower = radio_config.get('txpower', 'max')

        self.radio = radio

    def transmit(self, packet, txpower=None):
        """
        Transmit Packet

        :param bytes packet: buffer to send
        :return: if ACK received of ACK disabled
        :rtype: bool
        """

        self.next_tx_channel()

        if HOYMILES_TRANSACTION_LOGGING:
            c_datetime = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
            logging.debug(f'{c_datetime} Transmit {len(packet)} bytes channel {self.tx_channel}: {hexify_payload(packet)}')

        if not txpower:
            txpower = self.txpower

        inv_esb_addr = b'\01' + packet[1:5]
        dtu_esb_addr = b'\01' + packet[5:9]

        self.radio.stopListening()  # put radio in TX mode
        self.radio.setDataRate(RF24_250KBPS)
        self.radio.openReadingPipe(1,dtu_esb_addr)
        self.radio.openWritingPipe(inv_esb_addr)
        self.radio.setChannel(self.tx_channel)
        self.radio.setAutoAck(True)
        self.radio.setRetries(3, 15)
        self.radio.setCRCLength(RF24_CRC_16)
        self.radio.enableDynamicPayloads()

        if txpower == 'min':
            self.radio.setPALevel(RF24_PA_MIN)
        elif txpower == 'low':
            self.radio.setPALevel(RF24_PA_LOW)
        elif txpower == 'high':
            self.radio.setPALevel(RF24_PA_HIGH)
        else:
            self.radio.setPALevel(RF24_PA_MAX)

        return self.radio.write(packet)

    def receive(self, timeout=None):
        """
        Receive Packets

        :param timeout: receive timeout in nanoseconds (default: 5e8)
        :type timeout: int
        :yields: fragment
        """

        if not timeout:
            timeout=5e8

        self.radio.setChannel(self.rx_channel)
        self.radio.setAutoAck(False)
        self.radio.setRetries(0, 0)
        self.radio.enableDynamicPayloads()
        self.radio.setCRCLength(RF24_CRC_16)
        self.radio.startListening()

        fragments = []
        received_sth=False
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
                received_sth=True
                yield fragment

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
        
        if not received_sth:
            raise TimeoutError
        

    def next_rx_channel(self):
        """
        Select next channel from hop list
        - if hopping enabled
        - if channel has no ack

        :return: if new channel selected
        :rtype: bool
        """
        if not self.rx_channel_ack:
            self.rx_channel_id = self.rx_channel_id + 1
            if self.rx_channel_id >= len(self.rx_channel_list):
                self.rx_channel_id = 0
            return True
        return False

    def next_tx_channel(self):
        """
        Select next channel from hop list

        """
        self.tx_channel_id = self.tx_channel_id + 1
        if self.tx_channel_id >= len(self.tx_channel_list):
            self.tx_channel_id = 0

    @property
    def tx_channel(self):
        """
        Get current tx channel

        :return: tx_channel
        :rtype: int
        """
        return self.tx_channel_list[self.tx_channel_id]

    @property
    def rx_channel(self):
        """
        Get current rx channel

        :return: rx_channel
        :rtype: int
        """
        return self.rx_channel_list[self.rx_channel_id]

    def __del__(self):
        self.radio.powerDown()

def frame_payload(payload):
    """
    Prepare payload for transmission, append Modbus CRC16

    :param bytes payload: payload to be prepared
    :return: payload + crc
    :rtype: bytes
    """
    payload_crc = f_crc_m(payload)
    payload = payload + struct.pack('>H', payload_crc)

    return payload

def compose_esb_fragment(fragment, seq=b'\x80', src=99999999, dst=1, **params):
    """
    Build standart ESB request fragment

    :param bytes fragment: up to 16 bytes payload chunk
    :param seq: frame sequence byte
    :type seq: bytes
    :param src: dtu address
    :type src: int
    :param dst: inverter address
    :type dst: int
    :return: esb frame fragment
    :rtype: bytes
    :raises ValueError: if fragment size larger 16 byte
    """
    if len(fragment) > 17:
        raise ValueError(f'ESB fragment exeeds mtu: Fragment size {len(fragment)} bytes')

    packet = b'\x15'
    packet = packet + ser_to_hm_addr(dst)
    packet = packet + ser_to_hm_addr(src)
    packet = packet + seq

    packet = packet + fragment

    crc8 = f_crc8(packet)
    packet = packet + struct.pack('B', crc8)

    return packet

def compose_esb_packet(packet, mtu=17, **params):
    """
    Build ESB packet, chunk packet

    :param bytes packet: payload data
    :param mtu: maximum transmission unit per frame (default: 17)
    :type mtu: int
    :yields: fragment
    """
    for i in range(0, len(packet), mtu):
        fragment = compose_esb_fragment(packet[i:i+mtu], **params)
        yield fragment

def compose_send_time_payload(cmdId, alarm_id=0):
    """
    Build set time request packet

    :param cmd to request
    :type cmd: uint8
    :return: payload
    :rtype: bytes
    """
    timestamp = int(time.time())

    # indices from esp8266 hmRadio.h / sendTimePacket()
    payload = struct.pack('>B', cmdId)                # 10
    payload = payload + b'\x00'                       # 11
    payload = payload + struct.pack('>L', timestamp)  # 12..15 big-endian: msb at low address
    payload = payload + b'\x00\x00'                   # 16..17
    payload = payload + struct.pack('>H', alarm_id)   # 18..19
    payload = payload + b'\x00\x00\x00\x00'           # 20..23

    return frame_payload(payload)

class InverterTransaction:
    """
    Inverter transaction buffer, implements transport-layer functions while
    communicating with Hoymiles inverters
    """
    tx_queue = []
    scratch = []
    inverter_ser = None
    inverter_addr = None
    dtu_ser = None
    req_type = None
    time_rx = None

    radio = None
    txpower = None

    def __init__(self,
            request_time=None,
            inverter_ser=None,
            dtu_ser=None,
            radio=None,
            **params):
        """
        :param request: Transmit ESB packet
        :type request: bytes
        :param request_time: datetime of transmission
        :type request_time: datetime
        :param inverter_ser: inverter serial
        :type inverter_ser: str
        :param dtu_ser: DTU serial
        :type dtu_ser: str
        :param radio: HoymilesNRF instance to use
        :type radio: HoymilesNRF or None
        """

        if radio:
            self.radio = radio

            if 'txpower' in params:
                self.txpower = params['txpower']

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

        :return: if we got contact
        :rtype: bool
        """
        if not self.radio:
            return False

        if len(self.tx_queue) == 0:
            return False

        packet = self.tx_queue.pop(0)

        self.radio.transmit(packet, txpower=self.txpower)

        wait = False
        try:
            for response in self.radio.receive():
                if HOYMILES_TRANSACTION_LOGGING:
                    logging.debug(response)

                self.frame_append(response)
                wait = True
        except TimeoutError:
            pass
        except BufferError as e:
            logging.warning(f'Buffer error {e}')
            pass

        return wait

    def frame_append(self, frame):
        """
        Append received raw frame to local scratch buffer

        :param bytes frame: Received ESB frame
        :return None
        """
        self.scratch.append(frame)

    def queue_tx(self, frame):
        """
        Enqueue packet for transmission if radio is available

        :param bytes frame: ESB frame for transmit
        :return: if radio is available and frame scheduled
        :rtype: bool
        """
        if not self.radio:
            return False

        self.tx_queue.append(frame)

        return True

    def get_payload(self, src=None):
        """
        Reconstruct Hoymiles payload from scratch buffer

        :param src: filter frames by inverter hm_address (default self.inverter_address)
        :type src: bytes
        :return: payload
        :rtype: bytes
        :raises BufferError: if one or more frames are missing
        :raises ValueError: if assambled payload fails CRC check
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
            seq_last = max(frames, key=lambda frame:frame.seq).seq if len(frames) else 0
            self.__retransmit_frame(seq_last + 1)
            raise BufferError(f'Missing packet: Last packet {seq_last + 1}')

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

        :param int frame_id: frame id to re-schedule
        :return: if successful scheduled
        :rtype: bool
        """

        if not self.radio:
            return

        packet = compose_esb_fragment(b'',
                seq=int(0x80 + frame_id).to_bytes(1, 'big'),
                src=self.dtu_ser,
                dst=self.inverter_ser)

        return self.queue_tx(packet)

    def __str__(self):
        """
        Represent transmit payload

        :return: log line of payload for transmission
        :rtype: str
        """
        size = len(self.request)
        return f'Transmit | {hexify_payload(self.request)}'

def hexify_payload(byte_var):
    """
    Represent bytes

    :param bytes byte_var: bytes to be hexlified
    :return: two-byte while-space padded byte representation
    :rtype: str
    """
    return ' '.join([f"{b:02x}" for b in byte_var])
