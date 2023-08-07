#!/usr/bin/env python
# -*- coding: utf-8 -*-

from setuptools import Extension, setup

__version__ = '1.0.0'

bchlib_src = ['src/bchlib.c',
              'src/bch.c']
bchlib_dep = ['src/bch.h']

bchlib_ext = Extension('bchlib', bchlib_src, depends=bchlib_dep,
                       extra_compile_args=['-std=c99'])

setup(name='bchlib', version = __version__,
      ext_modules = [bchlib_ext],
      description = 'A python wrapper module for the kernel BCH library.',
      url = 'https://github.com/jkent/python-bchlib',
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
        'Programming Language :: Python :: 3',
        'Topic :: Security :: Cryptography',
        'Topic :: Software Development :: Libraries :: Python Modules',
      ],
)
