# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# This script is sourced by extra/ci_apply_tests.sh to customize the CI test
# list for this board variant. Three helper functions are available:
#   get_branch_tip <folder> <repo> <branch> [<path> ...]
#   get_latest_release <folder> <repo> [<path> ...]
#   skip_for_this_board <path-prefix>
#
# get_branch_tip / get_latest_release add external repos to the test list
# (the repos must have been downloaded by ci_fetch_tests.sh first).
# skip_for_this_board removes all tests under the given path prefix.

# Board-specific external repos
get_branch_tip libraries arduino-libraries/ArduinoBLE master \
	examples/Central/LedControl \
	examples/Central/Scan \
	examples/Peripheral/Advertising/EnhancedAdvertising \
	examples/Peripheral/ButtonLED \

get_branch_tip libraries arduino-libraries/Arduino_RouterBridge main \
	examples/client \
	examples/hci \
	examples/monitor \
	examples/server \
	examples/udp_ntp_client \

get_branch_tip libraries arduino-libraries/Arduino_RPClite main \
	examples/rpc_lite_client \
	examples/rpc_lite_server \

skip_for_this_board libraries/Arduino_RPClite/extras/integration_test
skip_for_this_board libraries/OTA
