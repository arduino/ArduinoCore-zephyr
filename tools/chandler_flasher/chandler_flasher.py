#!/usr/bin/env python3
"""Program PIC32CK flash (BFM or PFM) via pyOCD, driving the FCW directly.

pyOCD's built-in flash algorithm cannot write BFM: the LBWP/UBWP write-protect
bits re-arm at every reset, and the algorithm never performs the FCW key-unlock
dance, so writes are silently dropped.  For PFM the algorithm works (with
PFSWAP normalised and connect_mode=halt), but sector-erase timeouts and WDT
resets make it fragile.  This script sidesteps both by driving the FCW
registers directly, with the CPU held halted for the entire session.

The target region is selected automatically from --base:
  0x08000000-0x0801FFFF  BFM  (LBWP/UBWP unlock, blank-check after erase)
  0x0C000000-0x0CFFFFFF  PFM  (PWP0-7 unlock, PFSWAP normalisation)

Usage:
    source venv/bin/activate

    # BFM (MCUboot) - ELF auto-detects load address:
    ./tools/chandler_flasher.py build/mcuboot_.../zephyr/zephyr.elf

    # BFM (MCUboot) - raw binary:
    ./tools/chandler_flasher.py build/mcuboot_.../zephyr/zephyr.bin --base 0x08000000

    # PFM (loader) - always a raw binary, always needs --base:
    ./tools/chandler_flasher.py build/.../zephyr.signed.bin --base 0x0C000000

    # Verify only:
    ./tools/chandler_flasher.py --verify-only build/.../zephyr.bin --base 0x08000000

    # Erase entire BFM (no image needed):
    ./tools/chandler_flasher.py --erase-only --base 0x08000000
"""

import argparse
import sys
import time


# --- SoC constants (DS60001795, NVMCTRL chapter) ---------------------------

FCW = 0x4400_4000
CTRLA = FCW + 0x00
INTFLAG = FCW + 0x14
STATUS = FCW + 0x18
KEY = FCW + 0x1C
ADDR = FCW + 0x20
SRCADDR = FCW + 0x24
SWAP = FCW + 0x48
PWP0 = FCW + 0x4C   # PWP0..PWP7 at 0x4C..0x68
LBWP = FCW + 0x6C
UBWP = FCW + 0x70

UNLOCK = 0x91C3_2C00   # KEY[31:8], the valid unlock code
WRKEY = UNLOCK | 0x01   # unlocks CTRLA; does NOT persist across NVMOPs
SWAPKEY = UNLOCK | 0x02 # unlocks SWAP
CFGKEY = UNLOCK | 0x04  # unlocks LBWP/UBWP/PWPx/...; stays unlocked

NVMOP_ROW_WRITE = 0x3
NVMOP_PAGE_ERASE = 0x4

STATUS_BUSY = 1 << 0
INTFLAG_DONE = 1 << 0
INTFLAG_ERRORS = {
    1 << 1: "KEYERR",
    1 << 2: "CFGERR",
    1 << 3: "FIFOERR",
    1 << 4: "BUSERR",
    1 << 5: "WPERR",
    1 << 6: "OPERR",
    1 << 7: "SECERR",
    1 << 12: "RSTERR",
    1 << 13: "WRERR",
}
INTFLAG_ALL = INTFLAG_DONE | sum(INTFLAG_ERRORS)

SWAP_BFSWAP = 1 << 0
SWAP_PFSWAP = 1 << 8
SWAP_PFSLOCK = 1 << 9

# Geometry
BFM_BASE = 0x0800_0000
BFM_SIZE = 128 * 1024    # two 64K panels
PFM_BASE = 0x0C00_0000
PFM_SIZE = 1024 * 1024
PAGE_SIZE = 4096
ROW_SIZE = 1024
PWP_COUNT = 8

# 1 KB scratch SRAM for staging row data.  The CPU is halted, nothing owns it.
# Must be 16-byte aligned: the FCW ignores SRCADDR[3:0] and silently reads
# from the aligned-down address, shifting the data.
STAGE_ADDR = 0x2000_1000
assert STAGE_ADDR % 16 == 0, "STAGE_ADDR must be 16-byte aligned (FCW SRCADDR trap)"

TARGET_TYPE = "pic32ck1025sg01144"
TARGET_OPTIONS = {
    "pack.debug_sequences.enable": False,
    "connect_mode": "halt",
}

RETRIES = 2  # retry transient row-write / page-erase errors


class FcwError(RuntimeError):
    pass


# --- Region detection -------------------------------------------------------

def region_for(addr):
    """Return 'bfm' or 'pfm' based on address, or raise."""
    if BFM_BASE <= addr < BFM_BASE + BFM_SIZE:
        return "bfm"
    if PFM_BASE <= addr < PFM_BASE + PFM_SIZE:
        return "pfm"
    raise SystemExit(
        f"address {addr:#x} is outside both BFM ({BFM_BASE:#x}) and PFM ({PFM_BASE:#x})"
    )


# --- Image loading ----------------------------------------------------------

def load_image(path, base):
    """Return (start_address, bytes).

    ELF files: segments extracted by p_paddr.  Only accepted for BFM addresses;
    MCUboot-signed PFM images have headers that are not in the ELF, so taking
    segment addresses would place them wrong.

    Raw binaries: placed at ``base`` (required).
    """
    with open(path, "rb") as f:
        magic = f.read(4)
        f.seek(0)

        if magic == b"\x7fELF":
            if base is not None and region_for(base) == "pfm":
                raise SystemExit(
                    f"{path}: ELF given for a PFM address -- use the .bin or "
                    ".signed.bin and pass --base explicitly (the MCUboot image "
                    "header is not represented in the ELF)."
                )
            from elftools.elf.elffile import ELFFile

            segments = []
            for seg in ELFFile(f).iter_segments():
                if seg["p_type"] != "PT_LOAD" or seg["p_filesz"] == 0:
                    continue
                addr = seg["p_paddr"]
                if BFM_BASE <= addr < BFM_BASE + BFM_SIZE:
                    segments.append((addr, seg.data()))

            if not segments:
                raise SystemExit(
                    f"{path}: no PT_LOAD segment inside BFM ({BFM_BASE:#x})"
                )

            segments.sort()
            start = segments[0][0]
            end = max(a + len(d) for a, d in segments)
            blob = bytearray(b"\xff" * (end - start))
            for addr, data in segments:
                blob[addr - start : addr - start + len(data)] = data
            return start, bytes(blob)
        else:
            if base is None:
                raise SystemExit("--base is required for raw binary images")
            data = f.read()
            return base, data


# --- FCW driver -------------------------------------------------------------

class Fcw:
    """Direct driver for the PIC32CK Flash Controller (FCW/NVMCTRL).

    Bypasses the pyOCD flash algorithm entirely: unlock write-protect registers,
    erase pages, program rows, all as raw register pokes.  The CPU must be
    halted for the entire session.
    """

    def __init__(self, target):
        self.t = target

    def _wait_idle(self, timeout=2.0):
        deadline = time.time() + timeout
        while self.t.read32(STATUS) & STATUS_BUSY:
            if time.time() > deadline:
                raise FcwError("STATUS.BUSY stuck high")

    def _check_intflag(self, what, expect_done=True):
        flags = self.t.read32(INTFLAG)
        errs = [n for bit, n in INTFLAG_ERRORS.items() if flags & bit]
        if errs:
            raise FcwError(f"{what}: INTFLAG={flags:#010x} ({', '.join(errs)})")
        if expect_done and not (flags & INTFLAG_DONE):
            raise FcwError(
                f"{what}: INTFLAG={flags:#010x} -- no DONE and no error, "
                "the operation was silently ignored (KEY/write-protect problem)"
            )
        self.t.write32(INTFLAG, INTFLAG_ALL)  # write-1-to-clear

    def _nvmop(self, op, addr, srcaddr=None, what=""):
        self._wait_idle()
        self.t.write32(INTFLAG, INTFLAG_ALL)
        if srcaddr is not None:
            self.t.write32(SRCADDR, srcaddr)
        self.t.write32(ADDR, addr)
        # WRKEY unlocks CTRLA for exactly one operation -- rewrite every time.
        self.t.write32(KEY, WRKEY)
        self.t.write32(CTRLA, op)
        self._wait_idle()
        self._check_intflag(what or f"NVMOP {op:#x} @ {addr:#010x}")

    def page_erase(self, addr, retries=RETRIES):
        for attempt in range(1 + retries):
            try:
                self._nvmop(NVMOP_PAGE_ERASE, addr,
                            what=f"page erase @ {addr:#010x}")
                return
            except FcwError:
                if attempt == retries:
                    raise
                self.t.write32(INTFLAG, INTFLAG_ALL)

    def row_write(self, addr, data, retries=RETRIES):
        assert len(data) == ROW_SIZE and addr % ROW_SIZE == 0
        for attempt in range(1 + retries):
            try:
                self.t.write_memory_block8(STAGE_ADDR, data)
                self._nvmop(
                    NVMOP_ROW_WRITE, addr, srcaddr=STAGE_ADDR,
                    what=f"row write @ {addr:#010x}",
                )
                return
            except FcwError:
                if attempt == retries:
                    raise
                self.t.write32(INTFLAG, INTFLAG_ALL)

    # --- BFM unlock ---

    def unlock_bfm(self):
        """Clear every LBWP and UBWP bit.  They re-arm at each reset by design."""
        self._wait_idle()
        self.t.write32(INTFLAG, INTFLAG_ALL)
        self.t.write32(KEY, CFGKEY)
        self.t.write32(LBWP, 0x0000_0000)
        self.t.write32(UBWP, 0x0000_0000)
        readback = {"LBWP": self.t.read32(LBWP), "UBWP": self.t.read32(UBWP)}
        self._check_intflag("BWP unlock", expect_done=False)
        for name, value in readback.items():
            if value & 0xFFFF:
                lock = " (LOCKed -- only a reset can clear it)" \
                       if value & (1 << 31) else ""
                raise FcwError(
                    f"{name}={value:#010x} after unlock -- "
                    f"write-protect still armed{lock}"
                )
        return readback

    # --- PFM unlock ---

    def normalize_pfswap(self):
        """Clear PFSWAP so PFM addresses mean what they say.

        On a board whose BFM Dual-Boot slot has been promoted, the Boot ROM sets
        SWAP.PFSWAP at every reset, shifting all PFM addresses by half the array.
        """
        swap = self.t.read32(SWAP)
        if not (swap & SWAP_PFSWAP):
            return False
        if swap & SWAP_PFSLOCK:
            raise FcwError(
                f"PFSWAP is set and locked (SWAP={swap:#010x}); "
                "PFM addresses are shifted and cannot be corrected"
            )
        self._wait_idle()
        self.t.write32(KEY, SWAPKEY)
        self.t.write32(SWAP, swap & ~SWAP_PFSWAP)
        if self.t.read32(SWAP) & SWAP_PFSWAP:
            raise FcwError("PFSWAP would not clear")
        return True

    def unlock_pfm(self):
        """Clear all eight PFM write-protect regions (PWP0-7)."""
        self._wait_idle()
        self.t.write32(INTFLAG, INTFLAG_ALL)
        self.t.write32(KEY, CFGKEY)
        for i in range(PWP_COUNT):
            self.t.write32(PWP0 + 4 * i, 0)
        self._check_intflag("PWP unlock", expect_done=False)
        stuck = {
            f"PWP{i}": v
            for i in range(PWP_COUNT)
            if (v := self.t.read32(PWP0 + 4 * i)) & 0xFFFF
        }
        if stuck:
            raise FcwError(
                "write-protect still armed: "
                + ", ".join(f"{n}={v:#010x}" for n, v in stuck.items())
            )

    def bfswap_state(self):
        """Return True if BFSWAP is set (promoted board)."""
        return bool(self.t.read32(SWAP) & SWAP_BFSWAP)


# --- Verify -----------------------------------------------------------------

VERIFY_CHUNK = 4096


def verify(target, start, blob):
    """Read back and compare in chunks for robustness with large images."""
    off = 0
    while off < len(blob):
        n = min(VERIFY_CHUNK, len(blob) - off)
        actual = bytes(target.read_memory_block8(start + off, n))
        expected = blob[off:off + n]
        if actual != expected:
            for i, (a, b) in enumerate(zip(actual, expected)):
                if a != b:
                    addr = start + off + i
                    print(f"  MISMATCH at {addr:#010x}: read {a:#04x}, want {b:#04x}")
                    return False
        off += n
    return True


# --- Main -------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="Program PIC32CK BFM or PFM via pyOCD (direct FCW access).",
        epilog="The target region (BFM or PFM) is selected automatically from --base.",
    )
    ap.add_argument("image", nargs="?", default=None,
                    help="ELF (BFM only) or raw binary to program")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=None,
                    help="load address for a raw binary (default: from ELF segments)")
    ap.add_argument("--verify-only", action="store_true",
                    help="compare flash against the image, write nothing")
    ap.add_argument("--erase-only", action="store_true",
                    help="erase the pages the image covers, then stop "
                         "(with --base and no image: erase the entire region)")
    ap.add_argument("--reset", action="store_true",
                    help="reset the CPU after programming (best-effort)")
    ap.add_argument("--probe", help="probe unique ID, if more than one is attached")
    args = ap.parse_args()

    # --erase-only with --base and no image: erase the whole region
    if args.image is None:
        if not args.erase_only:
            ap.error("image is required (omit only with --erase-only --base)")
        if args.base is None:
            ap.error("--base is required when no image is given")
        region = region_for(args.base)
        region_base = BFM_BASE if region == "bfm" else PFM_BASE
        region_size = BFM_SIZE if region == "bfm" else PFM_SIZE
        start = region_base
        blob = b"\xff" * region_size
    else:
        start, blob = load_image(args.image, args.base)
        region = region_for(start)
        region_base = BFM_BASE if region == "bfm" else PFM_BASE
        region_size = BFM_SIZE if region == "bfm" else PFM_SIZE

    if start % ROW_SIZE:
        raise SystemExit(f"start address {start:#x} is not {ROW_SIZE}-byte aligned")
    if start + len(blob) > region_base + region_size:
        raise SystemExit(
            f"image runs past end of {region.upper()}: "
            f"{start:#x}+{len(blob)} > {region_base + region_size:#x}"
        )

    padded = blob + b"\xff" * (-len(blob) % ROW_SIZE)
    n_rows = len(padded) // ROW_SIZE
    first_page = (start - region_base) // PAGE_SIZE
    last_page = (start + len(padded) - 1 - region_base) // PAGE_SIZE
    n_pages = last_page - first_page + 1

    label = args.image or f"{region.upper()} (full region)"
    print(f"{label}: {len(blob)} bytes at "
          f"{start:#010x}-{start + len(blob):#010x} ({region.upper()})")
    print(f"  {n_rows} rows of {ROW_SIZE} B, {n_pages} pages of {PAGE_SIZE} B")

    from pyocd.core.helpers import ConnectHelper

    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.probe,
        target_override=TARGET_TYPE,
        options=TARGET_OPTIONS,
        blocking=False,
    )
    if session is None:
        raise SystemExit("no probe found")

    with session:
        target = session.target
        target.halt()

        fcw = Fcw(target)

        # --- PFM: normalise PFSWAP ---
        if region == "pfm":
            if fcw.normalize_pfswap():
                print("  PFSWAP was set; normalised (PFM addresses now unshifted)")

        # --- Verify only ---
        if args.verify_only:
            ok = verify(target, start, blob)
            print("verify:", "OK" if ok else "FAILED")
            if args.reset:
                target.reset()
            return 0 if ok else 1

        # --- Unlock ---
        if region == "bfm":
            bwp = fcw.unlock_bfm()
            parts = [f"{n} cleared (reads {v:#010x})" for n, v in bwp.items()]
            print("  " + ", ".join(parts))
            if fcw.bfswap_state():
                print("  note: BFSWAP is set (promoted board)")
        else:
            fcw.unlock_pfm()
            print("  PWP0..7 cleared")

        # --- Erase ---
        for p in range(first_page, last_page + 1):
            page_addr = region_base + p * PAGE_SIZE
            fcw.page_erase(page_addr)
            # Read back blank: a silently-ignored erase is the exact failure
            # mode this script exists for, and would be invisible when
            # reprogramming the same image that is already there.
            probe = bytes(target.read_memory_block8(page_addr, 64))
            if probe != b"\xff" * 64:
                raise FcwError(
                    f"page {p} @ {page_addr:#010x} not blank after erase"
                )
            print(f"\r  erased page {p - first_page + 1}/{n_pages} "
                  f"@ {page_addr:#010x}", end="", flush=True)
        print()

        if args.erase_only:
            print("erase-only: stopping before program")
            if args.reset:
                target.reset()
            return 0

        # --- Program ---
        for r in range(n_rows):
            fcw.row_write(
                start + r * ROW_SIZE,
                padded[r * ROW_SIZE : (r + 1) * ROW_SIZE],
            )
            print(f"\r  programmed {r + 1}/{n_rows} rows", end="", flush=True)
        print()

        # --- Verify ---
        ok = verify(target, start, blob)
        print("verify:", "OK" if ok else "FAILED")

        if args.reset:
            target.reset()
            print("CPU reset.")

    if not args.reset:
        print("Power-cycle the board (a debugger reset does not reliably "
              "restart this CPU).")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
