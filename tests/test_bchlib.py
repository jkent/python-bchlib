import bchlib
import hashlib
import os
import random

def test_bchlib():
    # create a bch object
    BCH_POLYNOMIAL = 8219
    BCH_BITS = 16
    bch = bchlib.BCH(BCH_POLYNOMIAL, BCH_BITS)

    # random data
    data = bytearray(os.urandom(512))

    # encode and make a "packet"
    ecc = bch.encode(data)
    packet = data + ecc

    # print hash of packet
    sha1_initial = hashlib.sha1(packet)

    def bitflip(packet):
        byte_num = random.randint(0, len(packet) - 1)
        bit_num = random.randint(0, 7)
        packet[byte_num] ^= (1 << bit_num)

    # make BCH_BITS errors
    for _ in range(BCH_BITS):
        bitflip(packet)

    # print hash of packet
    sha1_corrupt = hashlib.sha1(packet)

    # de-packetize
    data, ecc = packet[:-bch.ecc_bytes], packet[-bch.ecc_bytes:]

    # correct
    bitflips = bch.decode_inplace(data, ecc)

    # packetize
    packet = data + ecc

    # print hash of packet
    sha1_corrected = hashlib.sha1(packet)

    assert sha1_initial.digest() == sha1_corrected.digest()
