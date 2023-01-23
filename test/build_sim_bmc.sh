#!/bin/bash
git clone https://git.code.sf.net/p/openipmi/code openipmi-code
cd openipmi-code
./bootstrap
./configure
make

