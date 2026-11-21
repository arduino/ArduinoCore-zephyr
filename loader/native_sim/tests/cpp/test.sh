# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# The C++ standard library (libstdc++ containers and strings) works from a
# sketch.

run 0 "cpp: libstdc++ containers and strings" --sketch="$SKETCH"
expect "cpp: output" stdout '^cpp-ok'
