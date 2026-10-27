#!/usr/bin/env python3
import re
import sys


def extract_slot0_info(dts_file_path):
    try:
        with open(dts_file_path, "r") as f:
            content = f.read()
    except FileNotFoundError:
        sys.exit(1)

    # 1. Find the base address of flash0
    flash0_pattern = re.compile(r"flash0:.*?reg\s*=\s*<\s*(0x[0-9a-fA-F]+)", re.DOTALL)
    flash0_match = flash0_pattern.search(content)

    # 2. Find the relative offset AND dimension (size) of slot0_partition
    slot0_pattern = re.compile(
        r"slot0_partition[a-zA-Z0-9_@]*:.*?reg\s*=\s*<\s*(0x[0-9a-fA-F]+|[0-9]+)\s+(0x[0-9a-fA-F]+|[0-9]+)\s*>",
        re.DOTALL,
    )
    slot0_match = slot0_pattern.search(content)

    # 3. Find the erase block size (optional, might not be in all DTS)
    erase_pattern = re.compile(
        r"erase-block-size\s*=\s*<\s*(0x[0-9a-fA-F]+|[0-9]+)\s*>"
    )
    erase_match = erase_pattern.search(content)

    if flash0_match and slot0_match:
        flash0_addr = int(flash0_match.group(1), 16)
        slot0_offset = int(
            slot0_match.group(1), 16 if "0x" in slot0_match.group(1).lower() else 10
        )
        slot0_dim = int(
            slot0_match.group(2), 16 if "0x" in slot0_match.group(2).lower() else 10
        )

        absolute_addr = flash0_addr + slot0_offset

        if erase_match:
            erase_dim = int(
                erase_match.group(1), 16 if "0x" in erase_match.group(1).lower() else 10
            )
            erase_dim_str = str(erase_dim)
        else:
            erase_dim_str = "None"

        # Output format: ADDRESS DIMENSION ERASE_BLOCK_SIZE
        print(f"0x{absolute_addr:X} {slot0_dim} {erase_dim_str}")
        sys.exit(0)

    sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(1)
    extract_slot0_info(sys.argv[1])
