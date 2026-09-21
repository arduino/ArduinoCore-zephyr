# Camera Serial Viewer

Live preview for the camera, over the board's USB serial port.

Use it with the `CameraSerialStream` example sketch. The sketch sends each frame behind a
small header describing the frame, so the viewer picks up the resolution and pixel format
from the wire - change the mode in the sketch and nothing here needs editing.

## Usage

1. Open `examples/CameraSerialStream/CameraSerialStream.ino` and upload it to the board.

2. Close the Serial Monitor, or anything else holding the port open.

3. Install the dependencies and run the viewer:

   ```
   pip install -r requirements.txt
   ./CameraSerialViewer.py
   ```

Press `q` or `Escape` to quit.

| Option | Meaning |
|---|---|
| `-p`, `--port` | Serial port. Autodetected when omitted |
| `-c`, `--credits` | Frame requests kept outstanding (default 2) |
| `-s`, `--scale` | Display scale factor (default 2.0) |

The window title shows the frame rate, throughput, resolution, and counts of dropped
frames and resynchronisations. The same line is printed to the console once a second.

### Dependencies

`pyserial` and `Pillow`, plus `tkinter` for the window. tkinter is part of the standard
library but is packaged separately on some systems:

- Debian / Ubuntu: `apt install python3-tk`
- macOS with Homebrew: `brew install python-tk`
- pyenv: install `tcl-tk` first, then rebuild the interpreter

## Replaces the Processing viewer

This tool supersedes `CameraRawBytesVisualizer` and `CameraCaptureRawBytes`. That pair
needed the Processing IDE, hardcoded the resolution and pixel format in a second place
that had to be kept in sync with the sketch by hand, and had no framing - so a single
stray byte on the wire desynchronised it permanently. Here the geometry comes from the
frame header, and you can add `Serial.print()` calls to the sketch while debugging: the
viewer skips over them and carries on.

## Protocol

Every frame is preceded by a 20 byte header. All multi-byte fields are little-endian.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `sync` | Bytes `D5 AA 96 5A`, in that wire order |
| 4 | 1 | `version` | Protocol version, currently `1` |
| 5 | 1 | `format` | `0` = RGB565, `1` = GRAYSCALE8 (matches `CameraPixelFormat`) |
| 6 | 1 | `flags` | bit 0: payload is little-endian RGB565 (`byte_swap` was enabled). Other bits reserved, sent as 0 |
| 7 | 1 | reserved | Sent as 0 |
| 8 | 2 | `width` | Pixels |
| 10 | 2 | `height` | Pixels |
| 12 | 4 | `length` | Payload bytes following the header |
| 16 | 4 | `seq` | Frame counter, wraps at 2^32. Lets the host count dropped frames |

Commands from the host to the board, one byte each:

| Byte | Name | Meaning |
|---|---|---|
| `R` | request | Send exactly one frame. This is the only command the viewer uses |
| `S` | stream | Free-run until told to stop |
| `X` | stop | Leave streaming mode, return to request-per-frame |

### Notes on the design

**Flow control is credit-based.** The host sends `R` twice at startup and one more each
time it finishes reading a frame. Two outstanding requests keep the link busy - the board
captures the next frame while the host is still reading the previous one - while making
sure the board can never block writing to a host that has gone away.

**Resynchronisation.** The host scans for `sync`, then sanity-checks the header
(`version`, a known `format`, and `length` matching `width * height * bytes_per_pixel`)
before trusting it. The sync word is deliberately non-ASCII, so text printed by the sketch
can never contain it, and non-self-overlapping, so a failed match can advance a byte
without backtracking.

**No checksum.** USB already checksums and retransmits at the link layer, so silent
corruption is not the realistic failure; losing framing is, and `sync`, `length` and `seq`
cover that. The `version` field is there if this ever needs revisiting.

## Throughput

Expect roughly 7-20 fps at 320x240 RGB565. The limit is the core's 1024 byte serial TX
ring (`CONFIG_ARDUINO_API_SERIAL_BUFFER_SIZE`), not USB - the link itself is High Speed.
