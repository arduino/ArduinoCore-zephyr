# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Common libc, libm and atomic functions work from a sketch.

run 0 "libc: libc, libm and atomics" --sketch="$SKETCH"
expect "libc: snprintf output" stdout '^42-ok'
