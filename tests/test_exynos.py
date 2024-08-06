#!/usr/bin/env python

"""16-bit ECC encoder as in the Samsung S5PV210 and Exynos NAND controllers"""

import bchlib
import unittest

class Tests(unittest.TestCase):
    def test(self):
        ECC_POLY = 8219
        ECC_BITS = 16

        xor_data = bytearray(b'\x9A\xD7\xEF\x91\x88\x80\xFB\xF7' \
                            b'\x06\x3A\x5C\x9F\x49\x24\xD0\x75' \
                            b'\x02\xE3\x59\xE0\xE4\xBC\x1E\x20' \
                            b'\x70\x2E')

        def xor_ecc(ecc):
            new_ecc = bytearray()
            for a, b in zip(ecc, xor_data):
                new_ecc.append(a ^ b)
            return new_ecc

        bch = bchlib.BCH(ECC_BITS, prim_poly=ECC_POLY)
        ecc = bch.encode(b'\xFF' * 512)
        ecc = xor_ecc(ecc)

        assert ecc == b'\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF' \
                    b'\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF' \
                    b'\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF' \
                    b'\xFF\xFF'

if __name__ == '__main__':
    unittest.main()
