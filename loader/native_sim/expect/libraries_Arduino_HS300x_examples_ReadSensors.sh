# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# ReadSensors prints the readings of the HS300x every second: run it until
# the first block is printed. The emulated HS300x reads a constant 25 degrees
# and 50 % humidity.

run_until '^Humidity ' "ReadSensors: run" --sketch="$SKETCH"
expect "ReadSensors: temperature" stdout '^Temperature = 25\.00 °C$'
expect "ReadSensors: humidity" stdout '^Humidity    = 50\.00 %$'
