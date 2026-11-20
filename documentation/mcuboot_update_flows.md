# MCUboot update flows

How a board that boots the loader through MCUboot gets a new image, and which component decides
what at each step. It covers the loader USB DFU (`loader/dfu_flash_updates.c`), the MCUboot hooks
built into MCUboot from `loader/mcuboot_hooks/`, and the loader-side confirm
(`loader/mcuboot_confirm.c`).

The hooks assume a swap mode (`swap-using-offset`, `swap-using-move` or `swap-scratch`) with
`CONFIG_BOOT_BOOTSTRAP`. Each variant enables them in its `mcuboot.conf`:

| Option (MCUboot image) | Effect |
|---|---|
| `CONFIG_BOOT_IMAGE_ACCESS_HOOKS` | permanent requests overwrite slot0, test requests are swapped |
| `CONFIG_ARDUINO_MCUBOOT_DFU_BOOT_MODE` | the retention boot mode `BOOT_MODE_TYPE_BOOTLOADER` opens MCUboot's USB DFU, without a timeout |
| `CONFIG_ARDUINO_MCUBOOT_DOUBLE_TAP` | a second reset-button press within 500 ms sets that boot mode |
| `CONFIG_ARDUINO_MCUBOOT_DFU_FADE_LED` | fades the `mcuboot-fade-led0` LED while MCUboot waits in USB DFU |

## Entry points

| Trigger | Sets | Ends up in |
|---|---|---|
| 1200-bps touch on the sketch's serial port | `CONFIG_LOADER_DFU_MAGIC` at offset 0 of `retention2` | loader USB DFU (`CONFIG_USE_DFU_FLASH_UPDATES`) |
| Double tap of the reset button | boot mode `BOOTLOADER` | MCUboot USB DFU |
| No bootable image in slot0 | boot mode `BOOTLOADER`, warm reboot | MCUboot USB DFU |
| Sketch header not valid (`CONFIG_WAIT_FOR_DFU_IF_SKETCH_IS_NOT_VALID`) | — | loader USB DFU |

Software and watchdog resets never open the double-tap window: only a reset-pin reset does.

## 1. Boot decision (every reset)

```mermaid
flowchart TD
  RST(["reset"]) --> DT{"double tap?<br/>(cookie in retained_ram,<br/>pin reset only)"}
  DT -- "second tap within 500 ms" --> SETBM["boot mode = BOOTLOADER"]
  DT -- no --> BGH
  SETBM --> BGH{"boot_go_hook():<br/>boot mode == BOOTLOADER?<br/>(read and cleared once)"}

  BGH -- yes --> UPD{"swap in progress?"}
  UPD -- yes --> SKIP["DFU skipped:<br/>the interrupted swap runs first"] --> GO
  UPD -- no --> MDFU["MCUboot USB DFU<br/>waits forever, watchdog fed,<br/>optional fade LED"]
  MDFU -- "download done" --> GO
  BGH -- no --> GO

  GO{"slot1 request?"}
  GO -- "PERMANENT<br/>(magic + image_ok)" --> HOOK["boot_image_check_hook():<br/>slot0 reported invalid"]
  HOOK --> BS["BOOTSTRAP: slot1 copied over slot0,<br/>slot1 trailer erased"]
  GO -- "TEST" --> SW["swap slot0 and slot1<br/>(copy_done set: revertable)"]
  GO -- none --> VAL

  BS --> VAL{"slot0 hash + signature ok?"}
  SW --> VAL
  VAL -- no --> NOB["mcuboot_status_change(NO_BOOTABLE_IMAGE):<br/>boot mode = BOOTLOADER, warm reboot"] --> RST
  VAL -- yes --> LDR(["jump to the loader"])
```

A permanent request never leaves a backup: the old image is overwritten, so a bad download is
caught by the signature check and ends in MCUboot USB DFU, not in a revert.

## 2. USB upload through the loader DFU

```mermaid
flowchart TD
  subgraph SKETCH["Sketch running"]
    T(["1200-bps touch"]) --> MAG["_on_1200_bps():<br/>LOADER_DFU_MAGIC in retention2,<br/>system reset"]
  end

  subgraph MCUBOOT["MCUboot"]
    MAG --> B1["no request pending:<br/>slot0 validated, jump"]
  end

  subgraph LOADER["Loader (slot0)"]
    B1 --> INFO["retrieve_flash_info():<br/>sketch layout from the image header<br/>(USE_LOADER_AND_BINARY_SQUASHED)"]
    INFO --> Q{"retention2 magic?<br/>(read and cleared)"}
    Q -- no --> RUN(["sketch starts"])
    Q -- yes --> LDFU["loader USB DFU<br/>PID = USB_DEVICE_PID + DFU_PID_OFFSET"]
    LDFU -- "alt complete_image" --> A0["whole loader + sketch image<br/>into slot1 (flash_img)"]
    LDFU -- "alt sketch" --> A1["header block + sketch blocks<br/>into slot1, loader range skipped"]
    A0 --> REQ["boot_request_upgrade(PERMANENT)"]
    A1 --> REQ
    REQ --> RB["500 ms for the host status,<br/>USB shut down, cold reboot"]
  end

  RB --> BOOT["MCUboot: PERMANENT request<br/>→ flow 1, BOOTSTRAP overwrite"]
```

- `complete_image` carries the loader and the sketch: use it whenever the loader changes.
- `sketch` rewrites only the blocks after the loader. The image hash still covers the loader
  range, which is taken from what slot1 already holds: if slot1 does not hold the same loader
  (first upload after flashing with a probe, after a swap, after a loader change), MCUboot
  rejects the image and the previous one keeps running.

## 3. Test requests and the confirm

```mermaid
flowchart TD
  W(["new image written to slot1<br/>by an updater using img_mgmt"]) --> TR["boot_request_upgrade(TEST)"]
  TR --> R(["reboot"]) --> SW["MCUboot swaps:<br/>slot0 = new, slot1 = old,<br/>copy_done set"]
  SW --> RUN["new image runs unconfirmed"]
  RUN --> C{"boot_write_img_confirmed()<br/>before the next reset?"}
  C -- yes --> KEEP(["new image stays"])
  C -- "no (reset, crash, watchdog)" --> REV["MCUboot reverts the swap"] --> OLD(["previous image boots"])

  CP(["copy interrupted between<br/>its last trailer writes"]) --> ODD["slot0: magic set,<br/>image_ok and copy_done not set"]
  ODD --> MC["mcuboot_confirm.c at APPLICATION init:<br/>not revertable → confirmed"]
```

- The series itself only issues permanent requests (USB DFU). A test request comes from an updater
  that writes slot1 and requests a test upgrade, for example an MCUboot-based OTA.
- `mcuboot_confirm.c` confirms only images MCUboot cannot revert (`copy_done` not set); a real test
  image stays unconfirmed until the application confirms it.
