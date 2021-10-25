#!/usr/bin/env python

import binascii
import hashlib
import os
import random

import bchlib


# create a bch object
BIT_STRENGTH = 16
bch = bchlib.BCH(BIT_STRENGTH, m=13)
#bch = bchlib.BCH(BIT_STRENGTH, prim_poly=8219)
DATA_LEN = bch.n // 8 - (bch.ecc_bits + 7) // 8

print('ecc_bits: %d (ecc_bytes: %d)' % (bch.ecc_bits, bch.ecc_bytes))
print('m: %d' % (bch.m,))
print('n: %d (%d bytes)' % (bch.n, bch.n // 8))
print('prim_poly: 0x%x' % (bch.prim_poly,))
print('t: %d' % (bch.t,))

# random data
data = bytearray(os.urandom(DATA_LEN))

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
for _ in range(BIT_STRENGTH):
    bitflip(packet)

# print hash of packet
sha1_corrupt = hashlib.sha1(packet)
print('packet sha1: %s' % (sha1_corrupt.hexdigest(),))

# de-packetize
data, ecc = packet[:-bch.ecc_bytes], packet[-bch.ecc_bytes:]

# decode
bch.data_len = DATA_LEN
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
