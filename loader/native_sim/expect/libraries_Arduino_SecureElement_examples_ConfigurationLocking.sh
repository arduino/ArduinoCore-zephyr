# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# ConfigurationLocking prints the serial number of the emulated ECC608, then
# finds it already locked and stops.

# (the SE050 of the Nicla Vision and Portenta C33 is not emulated: only
# built there)
case "$FQBN" in *mimic=nicla_vision*|*mimic=portentac33*) return ;; esac

run_until '^SecureElement already locked!' "ConfigurationLocking: run" --sketch="$SKETCH"
expect "ConfigurationLocking: serial number" stdout '^SecureElement Serial Number = [0-9A-F]{18}$'
