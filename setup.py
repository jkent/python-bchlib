#!/usr/bin/env python
# -*- coding: utf-8 -*-

from distutils.core import Extension, setup

__version__ = '0.6'

bchlib_src = ['bchlib/bchlib.c',
              'bchlib/bch.c']
bchlib_dep = ['bchlib/bch.h']

bchlib_ext = Extension('bchlib', bchlib_src, depends=bchlib_dep,
                       extra_compile_args=['-std=c99'])

setup(name='bchlib', version = __version__,
      ext_modules = [bchlib_ext],
      description = 'A python wrapper module for the kernel BCH library.',
      author = 'Jeff Kent',
      author_email = 'jeff@jkent.net',
      maintainer = 'Jeff Kent',
      maintainer_email = 'jeff@jkent.net',
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
      ]
)

