# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script is sourced from extra/ci_test_list.sh to provide
# artifact-specific tests for Zephyr CI tests.
#
# Two helper functions are provided for easy GitHub queries:
# - get_branch_tip <repo> <branch> [<path> ...]
# - get_latest_release <repo> [<path> ...]
#
# By default, the whole project will be added to the test suite.
# When given additional path arguments, the functions will only
# copy artifacts under the provided paths.

if [ "$ARTIFACT" == "zephyr_contrib" ] ; then
	# Minimal safety test for Zephyr contrib boards
	get_branch_tip examples arduino/arduino-examples main \
		examples/01.Basics/Blink
else
	# Get a few core Arduino examples
	get_branch_tip examples arduino/arduino-examples main \
		examples/01.Basics/Blink \
		examples/01.Basics/AnalogReadSerial \
		examples/04.Communication/SerialPassthrough \

	# Smoke test for C++ features
	get_latest_release libraries arduino-libraries/Arduino_JSON \

	# Smoke test for SPI API compatibilty
	get_branch_tip libraries stefandz/M95M01 master
fi
