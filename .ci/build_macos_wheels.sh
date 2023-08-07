#!/usr/bin/env bash

set -e

brew update
brew outdated pyenv || brew upgrade pyenv
export PATH=~/.pyenv/shims:$PATH
for PYVER in "3.6.15" "3.7.12" "3.8.12" "3.9.7" "3.10.0"; do
  pyenv install ${PYVER}
  pyenv global ${PYVER}
  python -m pip install wheel
  python setup.py bdist_wheel
done
