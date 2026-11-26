# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# SimpleAccelerometer prints the acceleration of the BMI270 every time a
# sample is ready: run it until the first sample is printed. The emulated
# BMI270 is at 100 Hz with a constant (0.25, 0.50, 1.00) g on the raw axes.
# The library swaps and negates the axes on the Nano 33 BLE (its define is
# only set when posing as that board), so the values differ there.

run_until '^-?[0-9]' "SimpleAccelerometer: run" --sketch="$SKETCH"
expect "SimpleAccelerometer: sample rate" stdout '^Accelerometer sample rate = 99\.84 Hz$'

case "$FQBN" in
*mimic=nano33ble*)
	expect "SimpleAccelerometer: Nano 33 BLE axes" stdout '^-0\.50[[:space:]]-0\.25[[:space:]]1\.00$'
	;;
*)
	expect "SimpleAccelerometer: axes" stdout '^0\.25[[:space:]]0\.50[[:space:]]1\.00$'
	;;
esac
