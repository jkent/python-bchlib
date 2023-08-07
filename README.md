python-bchlib [![Build Status](https://travis-ci.com/jkent/python-bchlib.svg?branch=master)](https://travis-ci.com/jkent/python-bchlib)
=============

This is a python module for encoding and correcting data using [BCH codes](https://en.wikipedia.org/wiki/BCH_code).

## Requirements
  Python 3.6 or greater required.

## Installing the latest release:
    $ pip install bchlib

## Installing from source:
  Make sure you have python-dev setup.  For Windows, this means you need [Visual Studio 2015](https://stackoverflow.com/a/44290942/6844002).

    $ pip install .

## Module Documentation
bchlib.__BCH(__ t[, poly=None][, m=None][, swap_bits=False] __)__ → bch
> A constructor creates a BCH object with given `t` bit strength. At least one of `poly` and/or `m` must be provided. If `poly` is provided but `m` (Galois field order) is not, `m` will be calculated automatically.  If `m` is provided, between 5 and 15 inclusive, `poly` will be selected automatically.  `swap_bits` reverses the bit order within data and syndrome bytes.

bch.__encode(__ data[, ecc] __)__
> Encodes `data` with an optional starting `ecc`.  Result can be retrieved using `bch.ecc`.

bch.__decode(__ data[, recv_ecc][, calc_ecc][, syn][, msg_len] __)__ → nerr
> Corrects `data` using `ecc` and returns the number of detected errors or -1 if uncorrectable.

bch.__correct(__ data[, ecc] __)__
> Corrects `data` inplace after decoding. `data` and `ecc` must not be readonly.

bch.__compute_even_syn(__ syn __)__ → syn
> Computes even syndromes from odd ones. Takes and returns a sequence of t * 2 elements.

bch.__bits__
> A readonly field; the number of bits an ecc takes up.

bch.__bytes__
> A readonly field; the number of bytes an ecc takes up.

bch.__ecc__
> A readonly field; a bytes object of the last ecc result.

bch.__errloc__
> A readonly field; a tuple of error locations.

bch.__m__
> A readonly field; the Galois field order.

bch.__msg_len__
> A read/write field; the message length.  Reset to zero before decoding.

bch.__n__
> A readonly field; the maximum codeword size in bits.

bch.__nerr__
> A readonly field; the number of detected bit errors.  Negative means uncorrectable.

bch.__syn__
> A readonly field; a tuple of syndromes after performing a __correct()__ or __compute_even_syn()__ operation.

bch.__t__
> A readonly field; the number bit errors that can be corrected.

## Usage Example

```python
import bchlib
import hashlib
import os
import random

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
```
