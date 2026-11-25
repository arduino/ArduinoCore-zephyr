# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# RandomNumber prints a random number from the emulated ECC608 every second:
# run it until the first one is printed.

# (the SE050 of the Nicla Vision and Portenta C33 is not emulated: only
# built there)
case "$FQBN" in *mimic=nicla_vision*|*mimic=portentac33*) return ;; esac

run_until '^Random number = ' "RandomNumber: run" --sketch="$SKETCH"
expect "RandomNumber: random number" stdout '^Random number = [0-9]+$'
