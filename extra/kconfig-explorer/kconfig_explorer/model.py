# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

"""Loading and comparing exported <variant>.kconfig.json snapshots. Used by
both the CLI (for spot-checks) and the GUI (for the matrix/diff view)."""

import json
from pathlib import Path


def load_snapshot(path):
    with open(path) as f:
        return json.load(f)


def load_snapshots(path_or_dir):
    """Loads one file, or every *.kconfig.json file in a directory. Returns
    a dict keyed by the snapshot's "board" field."""

    path = Path(path_or_dir)
    if path.is_dir():
        paths = sorted(path.glob("*.kconfig.json"))
    else:
        paths = [path]

    snapshots = {}
    for p in paths:
        snap = load_snapshot(p)
        snapshots[snap["board"]] = snap
    return snapshots


def union_symbol_names(snapshots):
    names = set()
    for snap in snapshots.values():
        names.update(snap["symbols"].keys())
    return sorted(names)


def symbol_value(snapshot, name):
    sym = snapshot["symbols"].get(name)
    return None if sym is None else sym["value"]


def differing_symbols(snapshots):
    """Returns the set of symbol names whose value differs across the given
    snapshots (including symbols missing/hidden in some of them)."""

    differing = set()
    for name in union_symbol_names(snapshots):
        values = {symbol_value(snap, name) for snap in snapshots.values()}
        if len(values) > 1:
            differing.add(name)
    return differing
