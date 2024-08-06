#!/usr/bin/env python

import os
import random
import unittest

import bchlib


def bitflip(data):
    bit = random.randint(0, len(data) * 8 - 1)
    data[bit // 8] ^= 1 << (bit % 8)

class Tests(unittest.TestCase):
    def exercise(self, *args, **kwargs):
        bch = bchlib.BCH(*args, **kwargs)
        bits = bch.n - bch.ecc_bits

        # print(f'data:\t{bits} bits (usable bytes: {bits // 8})')
        # print(f'ecc: \t{bch.ecc_bits} bits (bytes: {bch.ecc_bytes})')
        # print(f'm:   \t{bch.m}')
        # print(f'n:   \t{bch.n} ({(bch.n + 7) // 8} bytes)')
        # print(f'poly:\t{bch.prim_poly}')
        # print(f't:   \t{bch.t}')

        data = bytearray(os.urandom(bits // 8))
        ecc = bch.encode(data)
        packet = data + ecc
        corrupted_packet = bytearray(packet)

        for _ in range(bch.t):
            bitflip(corrupted_packet)

        corrupted_data = corrupted_packet[:-bch.ecc_bytes]
        corrupted_ecc = corrupted_packet[-bch.ecc_bytes:]

        nerr = bch.decode(corrupted_data, corrupted_ecc)
        # print(f'nerr: \t{nerr}')
        assert(nerr >= 0)
        # print(bch.errloc)

        corrected_data = bytearray(corrupted_data)
        corrected_ecc = bytearray(corrupted_ecc)
        bch.correct(corrected_data, corrected_ecc)
        corrected_packet = corrected_data + corrected_ecc

        assert(corrected_packet == packet)

    def test_t_eq_6_285(self):
        for _ in range(1000):
            self.exercise(6, prim_poly=285)

    def test_t_eq_6_285_swap(self):
        for _ in range(1000):
            self.exercise(6, prim_poly=285, swap_bits=True)

    def test_t_eq_6_487(self):
        for _ in range(1000):
            self.exercise(6, prim_poly=487)

    def test_t_eq_6_487_swap(self):
        for _ in range(1000):
            self.exercise(6, prim_poly=487, swap_bits=True)

    def test_t_eq_12_17475(self):
        for _ in range(1000):
            self.exercise(12, prim_poly=17475)

    def test_t_eq_12_17475_swap(self):
        for _ in range(1000):
            self.exercise(12, prim_poly=17475, swap_bits=True)

    def test_t_eq_16(self):
        for _ in range(1000):
            self.exercise(16, m=13)

    def test_t_eq_16_swap(self):
        for _ in range(1000):
            self.exercise(16, m=13, swap_bits=True)

    def test_t_eq_32(self):
        for _ in range(1000):
            self.exercise(32, m=14)

    def test_t_eq_32_swap(self):
        for _ in range(1000):
            self.exercise(32, m=14, swap_bits=True)

    def test_t_eq_64(self):
        for _ in range(1000):
            self.exercise(64, m=15)

    def test_t_eq_64_swap(self):
        for _ in range(1000):
            self.exercise(64, m=15, swap_bits=True)

if __name__ == '__main__':
    unittest.main()
