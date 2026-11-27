# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# SimpleRTC prints the time every second: the emulated RTC starts already
# set, at 2025-10-21 12:00:00 (see loader/native_sim/rtc_set.c), and ticks
# with the simulated time.

run_until '^Time is: 2025-10-21 12:00:02$' "SimpleRTC: run" --sketch="$SKETCH"
expect "SimpleRTC: time" stdout '^Time is: 2025-10-21 12:00:00$'
expect "SimpleRTC: ticks" stdout '^Time is: 2025-10-21 12:00:01$'
