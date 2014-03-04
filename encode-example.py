#!/usr/bin/env python

"""16-bit ECC encoder as in the Samsung S5PV210 and Exynos NAND controllers"""

import bchlib
import sys

ECC_POLY = 8219
ECC_BITS = 16

xor_data = '\x9A\xD7\xEF\x91\x88\x80\xFB\xF7' + \
           '\x06\x3A\x5C\x9F\x49\x24\xD0\x75' + \
           '\x02\xE3\x59\xE0\xE4\xBC\x1E\x20' + \
           '\x70\x2E'

def xor_ecc(ecc):
  new_ecc = ''
  for a, b in zip(ecc, xor_data):
    new_ecc += chr(ord(a) ^ ord(b))
  return new_ecc

ecc = bchlib.bch(ECC_POLY, ECC_BITS)
code = ecc.encode('\xFF' * 512)
code = xor_ecc(code)

for c in code:
  sys.stdout.write('%02X ' % ord(c))
sys.stdout.write('\n')

