# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

"""Prepare the updater for upload, including the USB DFU sector padding."""

import sys
from pathlib import Path


def main():
    if len(sys.argv) not in (4, 5):
        sys.exit(f"usage: {sys.argv[0]} <input.bin> <output.bin> <padding> [<slot_size>]")

    image = Path(sys.argv[1]).read_bytes()
    padding = int(sys.argv[3], 0)
    if len(sys.argv) == 5 and len(image) + padding > int(sys.argv[4], 0):
        sys.exit("DFU updater exceeds slot size")

    Path(sys.argv[2]).write_bytes(b"\xff" * padding + image)


if __name__ == "__main__":
    main()
