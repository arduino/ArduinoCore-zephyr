# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# CalibrationRTC reads the calibration of the emulated RTC (0 at start),
# then sets it one step higher.

run_until '^Calibration updated to: 1$' "CalibrationRTC: run" --sketch="$SKETCH"
expect "CalibrationRTC: calibration" stdout '^Current calibration: 0$'
