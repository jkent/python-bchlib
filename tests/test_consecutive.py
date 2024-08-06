#!/usr/bin/env python

import bchlib
import unittest

class Tests(unittest.TestCase):
    def test(self):
        bch = bchlib.BCH(16, m=13)
        ecc1 = bch.encode(b'First')
        ecc2 = bch.encode(b'Second')
        assert(bch.decode(b'First', ecc1) == 0)
        assert(bch.decode(b'Second', ecc2) == 0)

if __name__ == '__main__':
    unittest.main()
