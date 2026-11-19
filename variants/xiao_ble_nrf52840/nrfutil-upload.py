#!/usr/bin/env python3
#
# Copyright (c) 2026 Seeed Technology Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
"""Upload helper for boards whose only programming entry point from stock
state is the factory Adafruit nRF52 bootloader's serial DFU mode
(e.g. Seeed Studio XIAO nRF52840).

The bootloader writes application images at a fixed address (the start of
its application region), so every upload sends one combined image: the
loader at --loader-addr, 0xFF padding up to --sketch-addr, then the
sketch. This script assembles the image, packs it into a DFU package
with adafruit-nrfutil and flashes it over the given serial port.

adafruit-nrfutil is not an Arduino-packaged dependency; install it with:

    pip install adafruit-nrfutil
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(
        description="Flash the combined loader+sketch image through the "
        "factory Adafruit nRF52 bootloader serial DFU.")
    parser.add_argument("--loader", required=True,
                        help="path of the loader binary")
    parser.add_argument("--sketch",
                        help="path of the sketch binary (omit to flash the loader only)")
    parser.add_argument("--port", required=True,
                        help="serial port of the DFU bootloader")
    parser.add_argument("--loader-addr", required=True, type=lambda x: int(x, 0),
                        help="address the bootloader writes the application at")
    parser.add_argument("--sketch-addr", type=lambda x: int(x, 0),
                        help="address of the user sketch partition")
    parser.add_argument("--dev-type", default="0x0052",
                        help="DFU dev-type reported to the bootloader (nRF52840)")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if shutil.which("adafruit-nrfutil") is None:
        print("error: adafruit-nrfutil was not found on this system.", file=sys.stderr)
        print("This board is flashed through its factory Adafruit nRF52", file=sys.stderr)
        print("bootloader, whose serial DFU protocol is driven by the", file=sys.stderr)
        print("adafruit-nrfutil tool. Install it with:", file=sys.stderr)
        print("    pip install adafruit-nrfutil", file=sys.stderr)
        return 1

    with open(args.loader, "rb") as f:
        loader = f.read()

    if args.sketch:
        if args.sketch_addr is None:
            print("error: --sketch requires --sketch-addr", file=sys.stderr)
            return 1
        gap = args.sketch_addr - args.loader_addr
        if len(loader) > gap:
            print("error: loader (%d bytes) does not fit before the sketch "
                  "partition (%d bytes available)" % (len(loader), gap), file=sys.stderr)
            return 1
        with open(args.sketch, "rb") as f:
            sketch = f.read()
        image = loader + b"\xff" * (gap - len(loader)) + sketch
        print("combined image: loader=%d sketch=%d total=%d bytes"
              % (len(loader), len(sketch), len(image)))
    else:
        image = loader
        print("loader-only image: %d bytes" % len(loader))

    with tempfile.TemporaryDirectory() as tmp:
        combined = os.path.join(tmp, "combined.bin")
        package = os.path.join(tmp, "combined.zip")
        with open(combined, "wb") as f:
            f.write(image)

        base = ["adafruit-nrfutil"] + (["--verbose"] if args.verbose else [])

        result = subprocess.run(
            base + ["dfu", "genpkg", "--dev-type", args.dev_type,
                    "--application", combined, package])
        if result.returncode != 0:
            print("error: failed to create the DFU package", file=sys.stderr)
            return 1
        print("DFU package created")

        result = subprocess.run(
            base + ["dfu", "serial", "-pkg", package,
                    "-p", args.port, "-b", "115200"])
        if result.returncode != 0:
            print("error: dfu serial upload failed", file=sys.stderr)
            return 1

    print("Upload completed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
