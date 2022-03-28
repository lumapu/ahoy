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
from datetime import datetime
from RF24 import RF24, RF24_PA_LOW, RF24_PA_MAX, RF24_250KBPS

radio = RF24(22, 0, 1000000)

# Master Address ('DTU')
dtu_ser = 99978563412  # identical to fc22's

# inverter serial numbers
#inv_ser = 444473104619  # identical to fc22's #99972220200
inv_ser = 114174608145  # my inverter

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


def on_receive(p):
    """
    Callback: get's invoked whenever a packet has been received.
    :param p: Payload of the received packet.
    """

    d = {}

    ts = datetime.utcnow()
    ts_unixtime = ts.timestamp()
    print(ts.isoformat(), end='Z ')

    # interpret content
    if p[0] == 0x95:
        src, dst, cmd = struct.unpack('>LLB', p[1:10])
        src_s = f'{src:08x}'
        dst_s = f'{dst:08x}'
        print(f'MSG src={src_s}, dst={dst_s}, cmd={cmd},  ', end=' ')
        if cmd==1:
            unknown1, u1, i1, p1, u2, i2, p2, unknown2 = struct.unpack(
                '>HHHHHHHH', p[10:26])
            print(f'u1={u1/10}V, i1={i1/100}A, p1={p1/10}W,  ', end='')
            print(f'u2={u2/10}V, i2={i2/100}A, p2={p2/10}W,  ', end='')
            print(f'unknown1={unknown1}, unknown2={unknown2}')
        elif cmd==2:
            uk1, uk2, uk3, uk4, uk5, u, f, p = struct.unpack(
                '>HHHHHHHH', p[10:26])
            print(f'u={u/10:.1f}V, f={f/100:.2f}Hz, p={p/10:.1f}W,  ', end='')
            print(f'uk1={uk1}, ', end='')
            print(f'uk2={uk2}, ', end='')
            print(f'uk3={uk3}, ', end='')
            print(f'uk4={uk4}, ', end='')
            print(f'uk5={uk5}')
        else:
            print(f'unknown cmd {cmd}')
    else:
        print(f'unknown frame id {p[0]}')


def main_loop():
    """
    Keep receiving on channel 3. Every once in a while, transmit a request
    to one of our inverters on channel 40.
    """

    print_addr(inv_ser)
    print_addr(dtu_ser)

    ctr = 1
    while True:
        radio.setChannel(3)
        radio.enableDynamicPayloads()
        radio.setAutoAck(False)
        radio.setPALevel(RF24_PA_MAX)
        radio.setDataRate(RF24_250KBPS)
        radio.openWritingPipe(ser_to_esb_addr(inv_ser))
        radio.flush_rx()
        radio.flush_tx()
        radio.openReadingPipe(1,ser_to_esb_addr(dtu_ser))
        #radio.openReadingPipe(1,ser_to_esb_addr(inv_ser))
        radio.startListening()

        if ctr<3:
            pass
            # radio.printPrettyDetails()

        t_end = time.monotonic_ns()+1e9
        while time.monotonic_ns() < t_end:
            has_payload, pipe_number = radio.available_pipe()
            if has_payload:
                size = radio.getDynamicPayloadSize()
                payload = radio.read(size)
                print(f"Received {size} bytes on pipe {pipe_number}: " +
                      " ".join([f"{b:02x}" for b in payload]))
                on_receive(payload)

        radio.stopListening()  # put radio in TX mode
        radio.setChannel(40)
        radio.openWritingPipe(ser_to_esb_addr(inv_ser))

        if ctr<3:
            pass
            # radio.printPrettyDetails()

        ts = int(time.time())
        payload = compose_0x80_msg(src_ser_no=dtu_ser, dst_ser_no=inv_ser, ts=ts)
        print(f"{ctr:5d}: len={len(payload)} | " + " ".join([f"{b:02x}" for b in payload]), 
            flush=True)
        radio.write(payload)  # will always yield 'True' because auto-ack is disabled
        ctr = ctr + 1




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
