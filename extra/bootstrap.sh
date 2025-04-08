#!/bin/bash

if [ ! -f platform.txt ]; then
  echo Launch this script from the root core folder as ./extra/bootstrap.sh
  exit 2
fi

python3 -m venv venv
source venv/bin/activate
pip install west
west init -l .
west update "$@"
west zephyr-export
pip install -r ../zephyr/scripts/requirements-base.txt
# download slim toolchain from https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.16.8

# add here the required blobs based on supported platforms
west blobs fetch hal_nxp
