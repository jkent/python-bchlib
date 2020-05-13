#!/usr/bin/env python
# -*- coding: utf-8 -*-

from setuptools import Extension, setup

__version__ = '0.14.0'

bchlib_src = ['src/bchlib.c',
              'src/bch.c']
bchlib_dep = ['src/bch.h']

bchlib_ext = Extension('bchlib', bchlib_src, depends=bchlib_dep,
                       extra_compile_args=['-std=c99'])

setup(name='bchlib', version = __version__,
      ext_modules = [bchlib_ext],
      description = 'A python wrapper module for the kernel BCH library.  A fork of https://github.com/jkent/python-bchlib that extends support for Galois Field orders up to 31.',
      url = 'https://github.com/Ben-Mathews/python-bchlib',
      author = 'Ben Mathews',
      author_email = 'beniam@yahoo.com',
      maintainer = 'Ben Mathews',
      maintainer_email = 'beniam@yahoo.com',
      license = 'GNU GPLv2',
      classifiers = [
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: GNU General Public License v2 (GPLv2)',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: C',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Topic :: Security :: Cryptography',
        'Topic :: Software Development :: Libraries :: Python Modules',
      ],
)

