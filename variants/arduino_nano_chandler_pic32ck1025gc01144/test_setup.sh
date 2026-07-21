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

skip_for_this_board libraries/ArduinoBLE
skip_for_this_board libraries/Arduino_SecureElement
skip_for_this_board libraries/Arduino_Video
skip_for_this_board libraries/Camera
skip_for_this_board libraries/Storage
skip_for_this_board libraries/WiFi
