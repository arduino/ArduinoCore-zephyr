# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Blink, for 3 simulated seconds: the LED pin (13) goes both high and low.

run 0 "Blink: run" --sketch="$SKETCH" -stop_at=3
expect "Blink: LED on" stdout '^\[gpio\] .* pin 13 = 1$'
expect "Blink: LED off" stdout '^\[gpio\] .* pin 13 = 0$'
