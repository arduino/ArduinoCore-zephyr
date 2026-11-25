# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# ColorSensor prints the colour read by the APDS9960 every second: run it
# until the first one is printed. The emulated APDS9960 reads a constant
# colour.

run_until '^b = ' "ColorSensor: run" --sketch="$SKETCH"
expect "ColorSensor: red" stdout '^r = 100$'
expect "ColorSensor: green" stdout '^g = 200$'
expect "ColorSensor: blue" stdout '^b = 300$'
