python-bchlib
=============

This is a python module for encoding and correcting data using [BCH codes](https://en.wikipedia.org/wiki/BCH_code).

## Requirements
  * python3 or python2.7
  * python3-dev or python-dev

## Building and Installing
__Python 3:__

    $ python3 setup.py build
    $ sudo python3 setup.py install

 __Python 2.7:__

    $ python setup.py build
    $ sudo python setup.py install

## Module Documentation
bchlib.__BCH(__ polynomial, bits __)__ → bch
> Constructor creates a BCH object with given `polynomial` and `bits` strength. The Galois field order is automatically determined from the `polynomial`.

bch.__encode(__ data[, ecc] __)__ → ecc
> Encodes `data` with an optional starting `ecc` and returns an ecc.

bch.__correct(__ data, ecc __)__ → ( bitflips, data, ecc )
> Corrects `data` using `ecc` and returns a tuple.

bch.__correct_inplace(__ data, ecc __)__ → bitflips
> Corrects `data` using `ecc` in place, returning the number of bitflips.

bch.__correct_syndromes(__ data, syndromes __)__ → ( data, num_bitflips )
> Corrects `data` using a sequence of `syndromes`, returning a tuple.

bch.__ecc_size__
> A readonly field, the number of bytes an ecc takes up.

bch.__syndromes__
> A readonly field, a tuple of syndromes after performing a correct operation.

## Usage Example

```python
import bchlib
import hashlib
import os
import random

# create a bch object
BCH_POLYNOMIAL = 8219
BCH_BITS = 16
bch = bchlib.BCH(BCH_POLYNOMIAL, BCH_BITS)

# random data
data = bytearray(os.urandom(512))

# encode and make a "packet"
ecc = bch.encode(data)
packet = data + ecc

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
data, ecc = packet[:-bch.ecc_size], packet[-bch.ecc_size:]

# correct
bitflip_count = bch.correct_inplace(data, ecc)
print('bitflips: %d' % (bitflip_count))

# packetize
packet = data + ecc

# print hash of packet
sha1_corrected = hashlib.sha1(packet)
print('sha1: %s' % (sha1_corrected.hexdigest(),))

print('matched' if sha1_initial == sha1_corrected else 'failed')
```