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
    *
    The inverters use a BCD representation of the last 8
    digits of their serial number, in reverse byte order, 
    followed by \x01.
    """
    return ser_to_hm_addr(s)[::-1] + b'\x01'

def compose_0x80_msg(dst_ser_no=72220200, src_ser_no=72220200, ts=None):
    """
    Create a valid 0x80 request with the given parameters, and containing the current system time.
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


def main_loop():
    """
    Keep receiving on channel 3. Every once in a while, transmit a request
    to one of our inverters on channel 40.
    """
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
            radio.printPrettyDetails()

        t_end = time.monotonic_ns()+1e9
        while time.monotonic_ns() < t_end:
            has_payload, pipe_number = radio.available_pipe()
            if has_payload:
                size = radio.getDynamicPayloadSize()
                payload = radio.read(size)
                print(f"Received {size} bytes on pipe {pipe_number}: " +
                      " ".join([f"{b:02x}" for b in payload]))

        radio.stopListening()  # put radio in TX mode
        radio.setChannel(40)
        radio.openWritingPipe(ser_to_esb_addr(inv_ser))

        if ctr<3:
            radio.printPrettyDetails()

        ts = int(time.time())
        payload = compose_0x80_msg(src_ser_no=dtu_ser, dst_ser_no=inv_ser, ts=ts)
        print(f"{ctr:5d}: len={len(payload)} | " + " ".join([f"{b:02x}" for b in payload]))
        radio.write(payload)  # will always yield 'True' because auto-ack is disabled
        ctr = ctr + 1




if __name__ == "__main__":

    if not radio.begin():
        raise RuntimeError("radio hardware is not responding")

    radio.setPALevel(RF24_PA_LOW)  # RF24_PA_MAX is default

    # radio.printDetails();  # (smaller) function that prints raw register values
    radio.printPrettyDetails();  # (larger) function that prints human readable data

    try:
        main_loop()

    except KeyboardInterrupt:
        print(" Keyboard Interrupt detected. Exiting...")
        radio.powerDown()
        sys.exit()
