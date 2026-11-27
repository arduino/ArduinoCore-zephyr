# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# ReadPressure prints the readings of the LPS22HB every second: run it until
# the first block is printed. The emulated LPS22HB reads a constant 100 kPa
# and 25 degrees.

run_until '^Altitude = ' "ReadPressure: run" --sketch="$SKETCH"
expect "ReadPressure: pressure" stdout '^Pressure = 100\.00 kPa$'
expect "ReadPressure: temperature" stdout '^Temperature = 25\.00 C$'
expect "ReadPressure: altitude" stdout '^Altitude = 110\.90 m$'
