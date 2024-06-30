#!/usr/bin/env python

import bchlib
import os
import random
import unittest

def bitflip(data, bits):
    bit = random.randint(8 - (bits % 8), bits)
    data[bit // 8] ^= 0x80 >> (bit % 8)

class BCHTestCase(unittest.TestCase):
    def test(self):
        bch = bchlib.BCH(6, prim_poly=285)
        correct_data = bytearray(b'\x7F' + os.urandom(25))
        correct_ecc = bch.encode(correct_data)
        packet = correct_data + correct_ecc

        for _ in range(bch.t):
            bitflip(packet, bch.n)

        damaged_data, damaged_ecc = packet[:-bch.ecc_bytes], packet[-bch.ecc_bytes:]
        nerr = bch.decode(damaged_data, damaged_ecc)
        bch.correct(damaged_data, damaged_ecc)
        assert(correct_data == damaged_data)
        assert(correct_ecc == damaged_ecc)

if __name__ == '__main__':
    unittest.main()
