# Copyright (c) 2026 Seeed Technology Inc.
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

# No variant-specific customization is needed: this board belongs to the
# zephyr_contrib artifact, where core library tests are already skipped at
# the artifact level (extra/artifacts/zephyr_contrib.test_setup.sh) and CI
# compiles only the Blink example.
