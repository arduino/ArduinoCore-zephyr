# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# M95M01_test writes and reads back the whole emulated M95M01 EEPROM three
# times (zeros, page-aligned and unaligned random data), then waits for a
# character on Serial: run it until then, and check that every byte read
# back as written.

run_until '^Send any serial char to repeat\.' "M95M01_test: run" --sketch="$SKETCH"
expect_none "M95M01_test: readback" stdout 'readback error'
