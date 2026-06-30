#!/bin/bash

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PATCH_DIR="$SCRIPT_DIR/patches/zephyr"

if [ ! -d "$PATCH_DIR" ]; then
	# No patch set is configured.
	exit 0
fi

if ! command -v west >/dev/null 2>&1; then
	echo "[ERROR] west is not available; cannot resolve Zephyr workspace"
	exit 1
fi

ZEPHYR_BASE="$(west list -f '{abspath}' zephyr 2>/dev/null || true)"
if [ -z "$ZEPHYR_BASE" ]; then
	echo "[ERROR] Unable to locate Zephyr module with 'west list zephyr'"
	exit 1
fi

if [ ! -d "$ZEPHYR_BASE/.git" ]; then
	echo "[ERROR] Zephyr repository not found at '$ZEPHYR_BASE'"
	exit 1
fi

applied_any=false
for patch in "$PATCH_DIR"/*.patch; do
	[ -e "$patch" ] || continue
	patch_name="$(basename "$patch")"

	if git -C "$ZEPHYR_BASE" apply --check "$patch" >/dev/null 2>&1; then
		echo "[INFO] Applying Zephyr patch: $patch_name"
		git -C "$ZEPHYR_BASE" apply "$patch"
		applied_any=true
	elif git -C "$ZEPHYR_BASE" apply --reverse --check "$patch" >/dev/null 2>&1; then
		echo "[INFO] Zephyr patch already applied: $patch_name"
	else
		echo "[ERROR] Cannot apply Zephyr patch cleanly: $patch_name"
		echo "[ERROR] Patch file: $patch"
		exit 1
	fi
done

if [ "$applied_any" = true ]; then
	echo "[INFO] Zephyr patch set applied"
fi
