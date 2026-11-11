#!/usr/bin/env bash
# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Run the native_sim tests: each directory here is a sketch that is
# compiled with arduino-cli, then its test.sh checks how the loader runs
# it. Exits with code 1 if any check failed.
#
# Usage: run_tests.sh [<test name>...]
#
# With no test name, all the tests are run. Only the environment variables
# of run_sketches.sh (SKETCH_REPORT) and common.sh apply.

set -u

TESTS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

if [ $# -eq 0 ] ; then
	set -- $(cd "$TESTS_DIR" && ls -d */ | tr -d /)
fi

exec "$TESTS_DIR/run_sketches.sh" -n "$TESTS_DIR" -p examples/loader/ "${@/#/$TESTS_DIR/}"
