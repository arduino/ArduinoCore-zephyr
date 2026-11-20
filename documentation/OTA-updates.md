# OTA updates (mezza)

This note explains how Arduino's mcuboot-based OTA mechanism works (as used by
the Nano XL Link board) and what mezza currently has in place after the OTA
groundwork port, plus the exact configuration changes required to actually
turn OTA on.

## Mechanism overview

mezza has two independent ways to install a new firmware image, both ending
up writing into the MCUboot secondary slot (`slot1_partition`):

- **USB DFU** (via `loader/dfu_flash_updates.c`, "complete_image" and
  "sketch" alternates, and via MCUboot's own built-in recovery DFU at
  PID `0x087B`, entered by double-tap). This is meant to be **permanent**:
  once mcuboot sees the pending image it replaces slot0 for good, no revert.
- **Network OTA** (`libraries/OTA` Arduino API + `loader/ota/ota_mcuboot.c`,
  the `OTA_BACKEND_MCUBOOT` backend). This is meant to be **tested**: the
  new image is swapped in on next boot, but if the sketch doesn't call
  `ota_confirm()` within a time window, a watchdog-driven reboot reverts to
  the previous image.

Both paths can coexist in the same firmware (this is exactly what Nano XL
Link does), because `loader/mcuboot_hooks/boot_hooks.c`'s
`boot_image_check_hook()` lets mcuboot's bootstrap logic tell a *permanent*
pending update (USB) from a *test* pending update (OTA) and treat each one
accordingly, instead of always doing a revertible swap.

To make this arbitration meaningful, mcuboot must run in
`MCUBOOT_MODE_SWAP_USING_OFFSET` rather than `MCUBOOT_MODE_OVERWRITE_ONLY`.
That mode reserves the first flash erase block of `slot1_partition` for
MCUboot's own swap-status bookkeeping, so the staged image itself starts one
erase block *into* slot1 instead of at offset 0
(`boot_get_image_start_offset()`).

A watchdog, started by MCUboot and fed by `loader/watchdog_feed_thread.c`, is
the safety net for OTA: while the running image isn't confirmed yet, the
feed thread watches `CONFIG_LOADER_TRIAL_WINDOW_MS`; if it expires (or the
sketch crashes, with `CONFIG_RESET_ON_FATAL_ERROR=y`) before `ota_confirm()`
is called, it force-reboots and MCUboot reverts the swap.

## What has already been ported to mezza (currently inert)

The following files/Kconfig options were added so the OTA feature is
buildable and ready to switch on, but **OTA is currently disabled and mezza's
behavior is unchanged**:

- `loader/slot1_trailer.{c,h}` — helper to erase a stale MCUboot trailer in
  slot1 before a new permanent/test update is staged.
- `loader/ota/*` — full `OTA_BACKEND_MCUBOOT` implementation
  (`ota_mcuboot.c`) plus the pre-existing filesystem backend.
- `libraries/OTA/*` — the sketch-facing API, including `ota_confirm()`.
- `loader/Kconfig` — `CONFIG_LOADER_TRIAL_WINDOW_MS`.
- `loader/watchdog_feed_thread.c` — confirm/revert trial-window logic.
- `loader/dfu_flash_updates.c` — both USB DFU alternates ("complete_image"
  and the squashed "sketch" alternate used via
  `CONFIG_USE_LOADER_AND_BINARY_SQUASHED`) now write at the correct offset
  and clear a stale trailer **when** `CONFIG_MCUBOOT_BOOTLOADER_MODE_SWAP_USING_OFFSET`
  is set. Today it isn't, so both alternates still write at offset 0 exactly
  as before.

None of this requires any change to enable later beyond Kconfig/sysbuild
settings described below — no further source changes are needed to turn OTA
on.

## How to enable OTA on mezza

Enabling OTA is a single, atomic configuration change across three files.
Doing only part of it will leave the board in an inconsistent state (e.g.
switching the swap mode without enabling the hooks would make USB DFU
revertible instead of permanent).

1. **`variants/arduino_mezza_stm32h5f5xx/sysbuild.conf`**
   ```
   SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=n
   SB_CONFIG_MCUBOOT_MODE_SWAP_USING_OFFSET=y
   SB_CONFIG_ENFORCE_SECURE_WATCHDOG=y
   SB_CONFIG_SECURE_WATCHDOG_TIMEOUT_MS=20000   # or whatever is appropriate
   ```

2. **`variants/arduino_mezza_stm32h5f5xx/mcuboot.conf`** — add:
   ```
   CONFIG_BOOT_BOOTSTRAP=y
   CONFIG_BOOT_IMAGE_ACCESS_HOOKS=y
   CONFIG_UPDATEABLE_IMAGE_NUMBER=1
   ```
   (`CONFIG_BOOT_IMAGE_ACCESS_HOOKS=y` activates `boot_hooks.c`, already
   present in `loader/mcuboot_hooks/` but currently inert because this symbol
   isn't set.) Review slot/sector sizing (`CONFIG_BOOT_MAX_IMG_SECTORS`) for
   mezza's actual flash/partition layout — this wasn't re-verified as part of
   this port.

3. **`variants/arduino_mezza_stm32h5f5xx/arduino_mezza_stm32h5f5xx.conf`** —
   add:
   ```
   CONFIG_OTA=y
   CONFIG_RESET_ON_FATAL_ERROR=y
   ```
   (`CONFIG_OTA_BACKEND_MCUBOOT` is selected automatically since
   `CONFIG_BOOTLOADER_MCUBOOT=y` is already set.)

After enabling this, verify:
- USB DFU (both the loader's "complete_image"/"sketch" alternates and
  MCUboot's own recovery DFU) still installs permanently — it should, via
  the hooks arbitration described above.
- A network OTA update actually requires the sketch to call `ota_confirm()`
  within `CONFIG_LOADER_TRIAL_WINDOW_MS` (default 60 s in swap-using-offset
  mode) or it is reverted on the next boot.
- `CONFIG_USE_LOADER_AND_BINARY_SQUASHED`'s sketch-only DFU alternate still
  lands the sketch bytes correctly (now one erase block further into
  slot1 than before).
