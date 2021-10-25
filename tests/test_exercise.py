#!/usr/bin/env python

import binascii
import hashlib
import os
import random
import unittest

import bchlib


class BCHTestCase(unittest.TestCase):
    def exercise(self, *args, **kwargs):
        # create a bch object
        bch = bchlib.BCH(*args, **kwargs)
        max_data_len = bch.n // 8 - (bch.ecc_bits + 7) // 8

        print('max_data_len: %d' % (max_data_len,))
        print('ecc_bits: %d (ecc_bytes: %d)' % (bch.ecc_bits, bch.ecc_bytes))
        print('m: %d' % (bch.m,))
        print('n: %d (%d bytes)' % (bch.n, bch.n // 8))
        print('prim_poly: 0x%x' % (bch.prim_poly,))
        print('t: %d' % (bch.t,))

        # random data
        data = bytearray(os.urandom(max_data_len))

        # encode and make a "packet"
        ecc = bch.encode(data)
        print('encoded ecc:', binascii.hexlify(ecc).decode('utf-8'))
        packet = data + ecc

        # print hash of packet
        sha1_initial = hashlib.sha1(packet)
        print('packet sha1: %s' % (sha1_initial.hexdigest(),))

        def bitflip(packet):
            byte_num = random.randint(0, len(packet) - 1)
            bit_num = random.randint(0, 7)
            packet[byte_num] ^= (1 << bit_num)

        # make BCH_BITS errors
        for _ in range(bch.t):
            bitflip(packet)

        # print hash of packet
        sha1_corrupt = hashlib.sha1(packet)
        print('packet sha1: %s' % (sha1_corrupt.hexdigest(),))

        # de-packetize
        data, ecc = packet[:-bch.ecc_bytes], packet[-bch.ecc_bytes:]

        # decode
        bch.data_len = max_data_len
        nerr = bch.decode(data, ecc)

        print('nerr:', nerr)
        print('syn:', bch.syn)
        print('errloc:', bch.errloc)

        # correct
        bch.correct(data, ecc)

        # packetize
        packet = data + ecc

        # print hash of packet
        sha1_corrected = hashlib.sha1(packet)
        print('packet sha1: %s' % (sha1_corrected.hexdigest(),))

        if sha1_initial.digest() == sha1_corrected.digest():
            print('Corrected!')
        else:
            print('Failed')

        assert sha1_initial.digest() == sha1_corrected.digest()

    def test_t_eq_6(self):
        self.exercise(6, prim_poly=487)

    def test_t_eq_12(self):
        self.exercise(12, prim_poly=17475, swap_bits=True)

    def test_t_eq_16(self):
        self.exercise(16, m=13)

    def test_t_eq_32(self):
        self.exercise(32, m=14)

    def test_t_eq_64(self):
        self.exercise(64, m=15)

if __name__ == '__main__':
    unittest.main()
