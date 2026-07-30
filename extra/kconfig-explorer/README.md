# Kconfig Explorer

Generate and browse the final Kconfig state of each `loader` build variant.

## How it works

`cli.py generate` runs `west build --cmake-only` for a board (configure
stage only, no compilation) and reads the `.config-trace.pickle` file that
`scripts/kconfig/kconfig.py` writes as part of that stage (Zephyr's
Kconfig-value-origin tracing feature). Each symbol's trace entry already
contains its final value and *why* it got that value (kind: `unset` /
`default` / `assign` / `select` / `imply`, plus a Kconfig/`.conf` file:line
or the list of active expressions responsible). This tool doesn't parse
CMakeCache.txt or invoke kconfiglib itself — the trace file already has
everything it needs.

The result is normalized into a snapshot file, named after the short
Arduino board name when known (e.g. `giga.kconfig.json`), falling back to
the full Zephyr variant name for raw targets that aren't a boards.txt
entry. The GUI only ever reads these snapshot files, so it has no
Zephyr/CMake/West dependency at runtime.

## CLI

```sh
# from the repo root, with the west venv active
./extra/kconfig-explorer/cli.py list
./extra/kconfig-explorer/cli.py generate giga --out /tmp/kconfig-state
./extra/kconfig-explorer/cli.py generate giga opta --out /tmp/kconfig-state
./extra/kconfig-explorer/cli.py generate --all --out /tmp/kconfig-state
./extra/kconfig-explorer/cli.py generate --all --out /tmp/kconfig-state --keep-build-dir
```

Each positional argument is a board name (from `boards.txt`) or a variant
name; anything that doesn't match either is passed straight through to
`west build -b` as a raw Zephyr target.

`--keep-build-dir` keeps the west build tree at `<out>/build/<variant>`
instead of a temp dir (useful for debugging a failed generation).

## GUI

```sh
pip install -r extra/kconfig-explorer/requirements.txt
./extra/kconfig-explorer/gui.py /tmp/kconfig-state
```

Opens a matrix with symbols as rows (grouped into a "SYMBOL (type)" row
header) and loaded variants as columns. Cell backgrounds show a
low(blue)-to-high(orange) value scale for bool/int/hex, green when a row is
identical across every loaded variant, and gray for symbols not present in
a given variant. Click a cell (or navigate with arrow keys) to see its
type/visibility/value and the reason (kind + location) in a floating
details panel that follows the selection and sizes itself to its content —
its text can be selected/copied, unlike a normal tooltip (Escape hides it).
Click a column header to collapse it to a small square (rows only present
in collapsed columns are hidden); dragging any other column resizes every
other non-collapsed column to match. Pass multiple files/folders as
arguments (`gui.py giga.kconfig.json opta.kconfig.json`, or a whole
directory) to load more variants as extra columns.

Filter bar: symbol name search, "Only user-set" (kind == assign, on by
default), "Hide logs" (hides `_LOG_LEVEL_` symbols, on by default), a
Diff dropdown (any / same / differing across loaded variants), and
type/kind filters.
