#!/usr/bin/env python

import bchlib
import hashlib
import os
import random

def test_1():
    # create a bch object
    BCH_POLYNOMIAL = 8219
    BCH_BITS = 16
    bch = bchlib.BCH(BCH_BITS, BCH_POLYNOMIAL)

    # random data
    data = bytearray(os.urandom(512))

    # encode and make a "packet"
    bch.encode(data)
    packet = data + bch.ecc

    # print hash of packet
    sha1_initial = hashlib.sha1(packet)
    print('sha1: %s' % (sha1_initial.hexdigest(),))

    def bitflip(packet):
        byte_num = random.randint(0, len(packet) - 1)
        bit_num = random.randint(0, 7)
        packet[byte_num] ^= (1 << bit_num)

    # make BCH_BITS errors
    for _ in range(BCH_BITS):
        bitflip(packet)

    # print hash of packet
    sha1_corrupt = hashlib.sha1(packet)
    print('sha1: %s' % (sha1_corrupt.hexdigest(),))

    # de-packetize
    data, ecc = packet[:-bch.bytes], packet[-bch.bytes:]

    # decode
    bch.decode(data, ecc)

    # correct
    bch.correct(data, ecc)

    # packetize
    packet = data + ecc

    # print hash of packet
    sha1_corrected = hashlib.sha1(packet)
    print('sha1: %s' % (sha1_corrected.hexdigest(),))

    if sha1_initial.digest() == sha1_corrected.digest():
        print('Corrected!')
    else:
        print('Failed')

    assert sha1_initial.digest() == sha1_corrected.digest()

if __name__ == '__main__':
    test_1()
