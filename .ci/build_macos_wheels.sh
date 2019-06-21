#!/usr/bin/env bash

set -e

brew update
brew outdated pyenv || brew upgrade pyenv
export PATH=~/.pyenv/shims:$PATH
for PYVER in "2.7.16" "3.4.10" "3.5.7" "3.6.8" "3.7.3"; do
  pyenv install ${PYVER}
  pyenv global ${PYVER}
  python -m pip install wheel
  python setup.py bdist_wheel
done
