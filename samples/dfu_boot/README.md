# DFU Sample Bootloader

> ## ⚠️ No longer in the Nano Chandler boot chain, 2026-08-17
>
> That board now boots MCUboot from BFM and uploads over mcumgr/SMP - see
> `documentation/nano_chandler_mcumgr.md`. This sample is kept because it
> still builds, because it is the reference implementation of the SoC's BFM
> Dual-Boot self-update (`src/bfm_flash.c`, `src/bfm_boot.c`, which the
> bootloader's own self-update will reuse), and because it remains a way to get
> a probe-less USB DFU path onto a board.
>
> Its `app.overlay` carries its own partition table, so it is unaffected by the
> variant's - but that table is the **old** map, and flashing this over MCUboot
> replaces the bootloader.

> **Based on Zephyr code sample:** `usb-dfu`
>
> Implement a basic USB DFU device to download/upload using dfu-util

## Overview

This sample is used in this repository as a USB DFU bootloader, using the USB device next stack.

## Requirements

This project requires a board with UDC API support.

## DFU Sample As First-Stage Bootloader

In this repository, this sample is the first-stage bootloader on Arduino Nano
Chandler. It no longer chainloads the loader directly: MCUboot sits between
them as a second stage, providing the A/B swap and rollback described in
`documentation/nano_chandler_ab_ota.md`. This sample is unaffected by that —
it just jumps to whatever `boot_partition` names.

The expected flash layout is:

* **BFM `0x08000000` (128 KB): this DFU sample (bootloader role).** BFM is the
  SoC's true hardware reset address, so the sample runs from there directly as
  one contiguous image — no `rom_start` stub/PFM-body split.
* `mcuboot` at `0x0C000000` (64 KB): the second stage, which this sample
  chainloads.
* `image_0` at `0x0C010000` (480 KB): MCUboot's primary slot, carrying the
  loader **and** the sketch as one image. `user_sketch` (`0x0C058000`, 176 KB)
  is a page-aligned window into the middle of it, so ordinary sketch uploads
  still work.
* `image_1` at `0x0C088000` (480 KB): MCUboot's secondary slot, OTA staging.

All 1024 KB of PFM is now allocated. The "dead first 256 KB" this file used to
describe is gone: it became the mcuboot region plus the start of slot0. See
`documentation/nano_chandler_ab_ota.md` for the full arrangement and for why the
sketch has to live inside the primary image.

`zephyr,code-partition` still has to name a PFM partition, because `IMG_MANAGER`
is mandatory here (Zephyr's `USBD_DFU_FLASH` hard-`depends on` it) and its
`boot_*` API writes MCUboot trailer magic to whatever that chosen node names.
It now aims at the **staging** partition, which is the least harmful target left
— a stray write there corrupts a re-downloadable OTA image, whereas every other
region is running code or the user's sketch.

Boot behavior is:

* On power-up/reset, DFU sample starts from boot flash.
* A double reset keeps the device in DFU mode.
* Without double reset, DFU sample checks if the loader image is present and
  jumps to it.

[`dfu-util`](https://dfu-util.sourceforge.net/) can be used to download the loader image into `slot0_image` and the application image into `slot1_image`.

### Application image requirements for chainload

The chainloaded image must include its vector table at the beginning of the
`mcuboot` (`0x0C000000`) section — this sample jumps to whatever
`boot_partition` names, which is MCUboot, not the loader.  
On PIC32CK this means it must not place
`rom_start` in Boot Flash Memory (`0x08000000`): it is achieved with KConfig `CONFIG_ROMSTART_RELOCATION_ROM=n`

### Build and package `Loader` and `Blink`

Build and flash the DFU bootloader to the board via JTAG/SWD.

```console
west build -b arduino_nano_chandler samples/dfu_boot
```

> **Plain `pyocd flash` does not work for this image**, but `pyocd` does.
> The image lives entirely in BFM, and every `LBWP` write-protect bit re-arms
> on each reset, so `pyocd flash`/`erase` against `0x08000000` prints a
> progress bar and "programmed N bytes" while changing nothing (see the
> `flash-chandler` skill and `extra/mcuboot/README.md`). Use
> `tools/chandler_flasher/chandler_flasher.py`, which clears `LBWP` via the FCW `CFGKEY` and then
> drives Page Erase + Row Write itself:
>
> ```console
> source venv/bin/activate
> ./tools/chandler_flasher/chandler_flasher.py build/dfu_boot_bfm/zephyr/zephyr.elf
> ```
>
> It reads each page back blank after erase and diffs the whole image at the
> end, so a silently-dropped write cannot pass unnoticed. Power-cycle
> afterwards — a debugger-issued reset does not reliably restart this CPU.
>
> J-Link remains a fine alternative; its own algorithm does the unlock itself
> (it reports `Bank 2 @ 0x08000000`):
>
> ```console
> JLinkExe -nogui 1 -if swd -speed 4000 -device PIC32CK1025GC
> J-Link> loadfile build/dfu_boot_nano_chandler/zephyr/zephyr.elf
> J-Link> r
> J-Link> g
> ```
>
> The `g` is required: `r` resets *and leaves the CPU halted*, so an `r`-only
> script looks like a dead board.

Build and download the Loader to `slot0_image` from DFU mode:

```console
./extra/build.sh nano_chandler

# .signed.bin, not .bin: slot0 holds an MCUboot image, so it needs the header.
dfu-util -d 2341:017e -a slot0_image -D build/arduino_nano_chandler_pic32ck1025gc01144/zephyr/zephyr.signed.bin
```

Build and download the Blink sketch to `slot1_image` from DFU mode:

```console
arduino-cli compile ~/Arduino/sketch/Blink/ -b arduino-git:zephyr:nano_chandler -v -e

dfu-util -d 2341:017e -a slot1_image -D ~/Arduino/sketch/Blink/build/arduino-git.zephyr.nano_chandler/Blink.ino.elf-zsk.bin
```

After download, reset the board. The DFU sample chainloads MCUboot from
`0x0C000000`, which in turn boots the loader out of slot0 and starts the sketch.

### Important Notes

#### Reset after download
A manual reset is required after executing the download. No automatic reset is done by the DFU sample, and the DfuSe `:leave` option can't be used here.

#### Entering DFU Mode
Two ways in, both going through the same cookie:

- **Double-tap reset.** An ordinary fast double-tap works: the cookie that
  detects it is armed from a `SYS_INIT(..., PRE_KERNEL_1, 0)` hook (see
  `dfu_double_reset_arm()` in `src/main.c`), so the window opens within
  microseconds of reset rather than after the sample has reached `main()`.
- **A USB DFU detach from the running loader or sketch** (`dfu-util -e`, and
  what `arduino-cli upload` does by itself). The loader/sketch writes the same
  cookie and resets, which is indistinguishable from a double-tap here — this
  sample needs no extra code path for it.

The cookie no longer lives in this image's `.noinit`: its address was private to
this image's link, and the loader could not reference it. It is now the
`DFU_COOKIE` reserved-memory region declared in the chandler variant's
`.overlay`, reached through `<dfu_cookie.h>`. That overlay is included by this
sample (see `CMakeLists.txt`) and by the loader and sketch builds, so all three
resolve the same address.

Because that region is carved out of `sram0`, **this sample and the loader must
be rebuilt and reflashed together** — see `documentation/nano_chandler_bfm.md`.

> Earlier revisions armed the cookie in `main()` and consequently needed the
> second tap "slightly later than the usual double-tap interval": everything
> before `main()` - driver/console bring-up, plus every boot log line, each a
> *blocking* UART write under `CONFIG_LOG_MODE_IMMEDIATE` - was a dead zone in
> which a second reset went undetected.

#### 1200 bps touch-reset
This sample exposes no CDC ACM port of its own, so it cannot observe a 1200 bps
open and **1200 bps touch-reset is not a way into this bootloader**.

The loader and sketch do handle a 1200 bps open, routing it to the same
`_on_1200_bps()` the DFU detach uses — so it lands here in DFU mode all the
same. `arduino-cli` does not rely on it: `nano_chandler.upload.use_1200bps_touch`
stays `false`, and the DFU detach is used instead.
