# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# AlarmRTC reads the time of the emulated RTC (set at 2025-10-21 12:00:00,
# see loader/native_sim/rtc_set.c), sets an alarm 5 seconds later and prints
# the time every second: the alarm goes off at 12:00:05.

run_until '^Alarm went off! Message: Wake up!!!' "AlarmRTC: run" --sketch="$SKETCH"
expect "AlarmRTC: current time" stdout '^Current Time: 2025-10-21 12:00:00$'
expect "AlarmRTC: alarm set" stdout '^Alarm set for: 12:00:05$'
expect "AlarmRTC: alarm time" stdout '^Time is: 2025-10-21 12:00:0[45]$'
