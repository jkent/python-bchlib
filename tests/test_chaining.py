#!/usr/bin/env python

import bchlib
import unittest

class Tests(unittest.TestCase):
    def test(self):
        bch = bchlib.BCH(16, m=13)
        ecc = bch.encode(b'First')
        ecc = bch.encode(b'Second', ecc=ecc)
        assert(bch.decode(b'FirstSecond', ecc) == 0)

if __name__ == '__main__':
    unittest.main()
