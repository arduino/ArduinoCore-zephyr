# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# AnalogReadSerial prints analogRead(A0) forever: the emulated ADC channel of
# A0 reads a constant 1650 mV of 3300 mV, i.e. 511 with 10 bits.

run_until '^[0-9]+$' "AnalogReadSerial: run" --sketch="$SKETCH"
expect "AnalogReadSerial: A0 reading" stdout '^511$'
