#!/bin/bash
set -e

source venv/bin/activate

# Get the variant name (NORMALIZED_BOARD_TARGET in Zephyr)
tmpdir=$(mktemp -d)
variant=$(cmake "-DBOARD=$1" -P extra/get_variant_name.cmake 2>/dev/null | grep 'VARIANT=' | cut -d '=' -f 2)
rm -rf ${tmpdir}

echo $variant
