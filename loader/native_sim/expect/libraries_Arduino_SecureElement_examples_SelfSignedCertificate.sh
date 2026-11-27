# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# SelfSignedCertificate prints the serial number of the emulated ECC608, then
# asks for the fields of the certificate on Serial: run it until it starts
# (the questions end with no newline; the signature is not emulated).

# (the SE050 of the Nicla Vision and Portenta C33 is not emulated: only
# built there)
case "$FQBN" in *mimic=nicla_vision*|*mimic=portentac33*) return ;; esac

run_until '^Hi there, in order to generate a new self signed cert' "SelfSignedCertificate: run" --sketch="$SKETCH"
expect "SelfSignedCertificate: serial number" stdout '^SecureElement Serial Number = [0-9A-F]{18}$'
