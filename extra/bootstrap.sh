#!/bin/bash
#
# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

set -e

log_msg() {
	if [ -n $GITHUB_WORKSPACE ] ; then
		echo "::$1::$2"
	else
		echo " - $2"
	fi
}

if [ ! -f platform.txt ]; then
  echo Launch this script from the root core folder as ./extra/bootstrap.sh
  exit 2
fi

NEEDED_HALS=$(grep 'build.zephyr_hals=' boards.txt | cut -d '=' -f 2 | xargs -n 1 echo | sort -u | xargs echo)

HAL_FILTER="-hal_.*"
for hal in $NEEDED_HALS; do
  HAL_FILTER="$HAL_FILTER,+$hal"
done

log_msg "group" "Bootstrapping Python environment for Zephyr"
python3 -m venv venv
source venv/bin/activate
pip install west protobuf grpcio-tools
log_msg "endgroup"

log_msg "group" "Initializing Zephyr workspace and modules: $HAL_FILTER"
west init -l .
west config manifest.project-filter -- "$HAL_FILTER"
west update "$@"
west zephyr-export
pip install -r ../zephyr/scripts/requirements-base.txt
log_msg "endgroup"

log_msg "group" "Installing Zephyr SDK 1.0.1"
west sdk install --version 1.0.1 -t arm-zephyr-eabi
log_msg "endgroup"

log_msg "group" "Fetching blobs for: $NEEDED_HALS"
west blobs fetch $NEEDED_HALS
log_msg "endgroup"
