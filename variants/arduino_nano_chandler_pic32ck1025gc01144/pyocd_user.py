# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# pyOCD user script for Nano Chandler (PIC32CK1025GC01144).
# Normalises PFSWAP (Boot ROM flips it alongside BFSWAP after a Dual-Boot
# promotion) and clears PWP0..7 so pyocd load can write PFM.
# BFM needs tools/chandler_flasher/chandler_flasher.py (LBWP/UBWP re-arm at reset).

FCW = 0x4400_4000
CTRLA = FCW + 0x00
STATUS = FCW + 0x18
KEY = FCW + 0x1C
SWAP = FCW + 0x48
PWP0 = FCW + 0x4C

UNLOCK = 0x91C3_2C00
CFGKEY = UNLOCK | 0x04
SWAPKEY = UNLOCK | 0x02

STATUS_BUSY = 1 << 0
SWAP_PFSWAP = 1 << 8
SWAP_PFSLOCK = 1 << 9


def did_connect(board):
    target = board.target

    swap = target.read32(SWAP)
    if swap & SWAP_PFSWAP:
        if swap & SWAP_PFSLOCK:
            print("pyocd_user: PFSWAP is set and LOCKED - PFM addresses stay "
                  "shifted, this flash would land in the wrong panel")
            return
        while target.read32(STATUS) & STATUS_BUSY:
            pass
        # Preserve BFSWAP: it selects which BFM slot boots.
        target.write32(KEY, SWAPKEY)
        target.write32(SWAP, swap & ~SWAP_PFSWAP)
        now = target.read32(SWAP)
        if now & SWAP_PFSWAP:
            print(f"pyocd_user: PFSWAP would not clear (SWAP={now:#010x})")
        else:
            print(f"pyocd_user: PFSWAP normalized (SWAP={swap:#010x} -> {now:#010x})")

    target.write32(KEY, CFGKEY)
    for i in range(8):
        target.write32(PWP0 + 4 * i, 0)
