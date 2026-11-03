#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

import re
import sys


def to_int(value):
    return int(value, 16 if "0x" in value.lower() else 10)


def find_matching_brace(content, open_brace_idx):
    """Return the index of the '{' at open_brace_idx's matching '}'."""
    depth = 0
    for i in range(open_brace_idx, len(content)):
        if content[i] == "{":
            depth += 1
        elif content[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return -1


def find_flash_node_containing_slot0(content):
    """Find the body of whichever 'flash@...' node holds slot0_partition.

    slot0 doesn't necessarily live under a node labelled 'flash0': boards with
    more than one flash device (e.g. a separate bootloader flash and a PFM/BFM
    flash) may place it under 'flash1' or any other flash node. Locate the
    flash node that actually contains the slot0_partition definition instead
    of assuming a fixed label.
    """
    flash_node_pattern = re.compile(r"\w+:\s*flash@[0-9a-fA-F]+\s*\{")
    for match in flash_node_pattern.finditer(content):
        open_brace_idx = content.index("{", match.start())
        close_brace_idx = find_matching_brace(content, open_brace_idx)
        if close_brace_idx == -1:
            continue
        node_body = content[match.start() : close_brace_idx + 1]
        if re.search(r"slot0_partition[a-zA-Z0-9_@]*:", node_body):
            return node_body
    return None


def extract_slot0_info(dts_file_path):
    try:
        with open(dts_file_path, "r") as f:
            content = f.read()
    except FileNotFoundError:
        sys.exit(1)

    # 1. Find the flash node that actually contains slot0_partition, so the
    # base address and erase-block-size are read from the right flash device
    # (slot0 is not always in the node labelled flash0).
    flash_node = find_flash_node_containing_slot0(content)
    if flash_node is None:
        sys.exit(1)

    # 2. Find the base address of that flash node (its own "reg", i.e. the
    # first one in the node, before any "partitions" sub-node).
    flash_addr_pattern = re.compile(r"reg\s*=\s*<\s*(0x[0-9a-fA-F]+)", re.DOTALL)
    flash_addr_match = flash_addr_pattern.search(flash_node)

    # 3. Find the relative offset AND dimension (size) of slot0_partition,
    # scoped to the flash node found above (not the whole DTS), so an
    # unrelated slot0-like node elsewhere can't be picked up by mistake.
    slot0_pattern = re.compile(
        r"slot0_partition[a-zA-Z0-9_@]*:.*?reg\s*=\s*<\s*(0x[0-9a-fA-F]+|[0-9]+)\s+(0x[0-9a-fA-F]+|[0-9]+)\s*>",
        re.DOTALL,
    )
    slot0_match = slot0_pattern.search(flash_node)

    # 4. Find the erase block size (optional, might not be in all DTS),
    # scoped to the flash node that owns slot0.
    erase_pattern = re.compile(
        r"erase-block-size\s*=\s*<\s*(0x[0-9a-fA-F]+|[0-9]+)\s*>"
    )
    erase_match = erase_pattern.search(flash_node)

    if flash_addr_match and slot0_match:
        flash_addr = to_int(flash_addr_match.group(1))
        slot0_offset = to_int(slot0_match.group(1))
        slot0_dim = to_int(slot0_match.group(2))

        absolute_addr = flash_addr + slot0_offset

        if erase_match:
            erase_dim_str = str(to_int(erase_match.group(1)))
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
