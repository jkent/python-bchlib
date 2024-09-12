#!/usr/bin/env python

import os

from setuptools import Extension, setup

NAME = 'bchlib'
VERSION = '2.1.3'
DESCRIPTION = 'A python wrapper module for the Linux kernel BCH library.'
URL = 'https://github.com/jkent/python-bchlib'
EMAIL = 'jeff@jkent.net'
AUTHOR = 'Jeff Kent'
REQUIRES_PYTHON = '>=3.6.0'

BCHLIB_EXT_SRC = [
    'src/bch.c',
    'src/bchlib.c',
]

BCHLIB_EXT_DEP = [
    'src/bch.h',
]

BCHLIB_EXT = Extension('bchlib', BCHLIB_EXT_SRC, depends=BCHLIB_EXT_DEP,
                       extra_compile_args=['-std=c99'])

here = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(here, 'README.md'), encoding='utf-8') as f:
    long_description = '\n' + f.read()

setup(
    name=NAME,
    version=VERSION,
    description=DESCRIPTION,
    long_description=long_description,
    long_description_content_type='text/markdown',
    author=AUTHOR,
    author_email=EMAIL,
    python_requires=REQUIRES_PYTHON,
    url=URL,
    ext_modules=[BCHLIB_EXT],
    include_package_data=True,
    classifiers=[
        'License :: OSI Approved :: GNU General Public License v2 (GPLv2)',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: Implementation :: CPython',
        'Programming Language :: Python :: Implementation :: PyPy',
        'Intended Audience :: Developers',
        'Topic :: Security :: Cryptography',
    ],
)
