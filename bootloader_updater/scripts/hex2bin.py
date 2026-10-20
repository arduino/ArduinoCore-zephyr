#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

"""
Extract the bootloader from an Intel HEX file as a raw binary.

The image starts at the lowest address in the file and is limited to the boot
partition size; data outside that range (e.g. option bytes) is dropped.
"""

import sys

WRITE_ALIGN = 256


def parse_hex(path):
    data = {}
    base = 0
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line.startswith(":"):
                continue
            raw = bytes.fromhex(line[1:])
            count, addr, rtype = raw[0], (raw[1] << 8) | raw[2], raw[3]
            payload = raw[4 : 4 + count]
            if rtype == 0x00:
                for i, b in enumerate(payload):
                    data[base + addr + i] = b
            elif rtype == 0x02:
                base = int.from_bytes(payload, "big") << 4
            elif rtype == 0x04:
                base = int.from_bytes(payload, "big") << 16
            elif rtype == 0x01:
                break
    return data


def main():
    if len(sys.argv) != 4:
        sys.exit(f"usage: {sys.argv[0]} <input.hex> <output.bin> <max_size>")

    data = parse_hex(sys.argv[1])
    max_size = int(sys.argv[3], 0)
    if not data:
        sys.exit(f"{sys.argv[1]}: no data")

    start = min(data)
    end = max(a for a in data if a < start + max_size) + 1
    end = min(start + max_size, (end - start + WRITE_ALIGN - 1) // WRITE_ALIGN * WRITE_ALIGN + start)

    image = bytes(data.get(a, 0xFF) for a in range(start, end))
    with open(sys.argv[2], "wb") as f:
        f.write(image)


if __name__ == "__main__":
    main()
