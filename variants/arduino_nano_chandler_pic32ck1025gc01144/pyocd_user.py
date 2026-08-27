# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# pyOCD user script for the Nano Chandler (PIC32CK1025GC01144).
#
# Passed with --script (see pyocd.extra_flags in boards.txt) so that an ordinary
# `pyocd load` - and therefore `arduino-cli burn-bootloader` - can write PFM on
# this part. Two things have to be true before the first flash access, and
# neither is true out of reset on every board:
#
#   PFSWAP  On a board whose BFM Dual-Boot slot has been promoted, the Boot ROM
#           sets SWAP.PFSWAP alongside SWAP.BFSWAP at every reset, and every PFM
#           address moves by half the array. The running firmware normalises it
#           (boot/arduino/hooks/pfswap.c (mcuboot fork)), but a debugger that connects
#           under-reset gets there first and would program the other panel - the
#           write and its read-back agreeing with each other the whole time.
#   PWP0..7 PFM write protect. Not armed out of reset on this board, but cleared
#           anyway so this does not silently depend on that.
#
# BFM is a different matter: its LBWP/UBWP bits re-arm at every reset and the
# borrowed flash algorithm cannot program it. Use tools/chandler_flasher/chandler_flasher.py.

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
