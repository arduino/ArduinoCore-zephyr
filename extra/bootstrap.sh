#!/bin/bash

if [ ! -f platform.txt ]; then
  echo Launch this script from the root core folder as ./extras/bootstrap.sh
  exit 2
fi

python3 -m venv venv
source venv/bin/activate
pip install west
west init -l .
west update
# download slim toolchain from https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.16.8