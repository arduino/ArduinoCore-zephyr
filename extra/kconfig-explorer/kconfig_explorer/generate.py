# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

"""Generates a <variant>.kconfig.json snapshot for a board by running
'west build --cmake-only' (configure stage only, no compilation) and reading
the .config-trace.pickle file that scripts/kconfig/kconfig.py writes as part
of that stage. No CMakeCache.txt parsing and no kconfiglib usage happens
here: the trace file already contains everything this tool needs (name,
visibility, type, value, kind, location) for every reachable symbol."""

import datetime
import json
import os
import pickle
import signal
import subprocess
import tempfile
from pathlib import Path

from . import boards

REPO_ROOT = boards.REPO_ROOT
LOADER_DIR = REPO_ROOT / "loader"

TRACE_RELATIVE_PATH = Path("zephyr") / ".config-trace.pickle"


class GenerationError(Exception):
    pass


def _normalize_location(kind, loc):
    if loc is None:
        return None
    if isinstance(loc, tuple):
        file, line = loc
        return {"kind": "fileline", "file": file, "line": line}
    if isinstance(loc, list):
        return {"kind": "exprs", "value": list(loc)}
    raise GenerationError(
        f"unrecognized location shape for kind={kind!r}: {loc!r}"
    )


def normalize_trace(trace_data):
    """Converts the list of (name, visibility, type, value, kind, location)
    tuples from .config-trace.pickle into the JSON-serializable symbols dict
    used in exported snapshots."""

    symbols = {}
    for name, visibility, sym_type, value, kind, loc in trace_data:
        symbols[name] = {
            "visibility": visibility,
            "type": sym_type,
            "value": value,
            "kind": kind,
            "location": _normalize_location(kind, loc),
        }
    return symbols


def build_snapshot(variant, board, target, trace_data, generated_at=None):
    return {
        "variant": variant,
        "board": board,
        "target": target,
        "generated_at": generated_at
        or datetime.datetime.now().isoformat(timespec="seconds"),
        "symbols": normalize_trace(trace_data),
    }


def _kill_process_group(process):
    """Terminates 'process' and every process it spawned (west forks cmake,
    which forks compiler-check subprocesses), used to make sure Ctrl-C
    actually stops the whole build instead of leaving it running in the
    background."""

    try:
        pgid = os.getpgid(process.pid)
    except ProcessLookupError:
        return

    try:
        os.killpg(pgid, signal.SIGTERM)
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(pgid, signal.SIGKILL)
        process.wait()
    except ProcessLookupError:
        pass


def run_cmake_only(target, args, build_dir):
    cmd = [
        "west", "build",
        "-d", str(build_dir),
        "-b", target,
        str(LOADER_DIR),
        "--cmake-only",
    ]
    if args:
        cmd += args.split()

    # New session so the child (and its cmake/compiler-check grandchildren)
    # can be killed as a group on Ctrl-C instead of only the direct child.
    process = subprocess.Popen(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate()
    except KeyboardInterrupt:
        _kill_process_group(process)
        raise

    if process.returncode != 0:
        raise GenerationError(
            f"'west build --cmake-only' failed for target {target!r}:\n"
            f"{stdout}\n{stderr}"
        )


def load_trace_pickle(build_dir):
    trace_path = Path(build_dir) / TRACE_RELATIVE_PATH
    if not trace_path.exists():
        raise GenerationError(
            f"trace file not found: {trace_path}\n"
            "(requires a zephyr checkout with the Kconfig trace-data feature)"
        )
    with open(trace_path, "rb") as f:
        return pickle.load(f)


def generate_variant(board_name, board_list, out_dir, keep_build_dir=False):
    """Generates <out_dir>/<board>.kconfig.json for the given board or
    variant name (a boards.txt entry). Returns the output path."""

    spec = boards.resolve_board(board_name, board_list)
    target = spec["target"]
    variant = spec["variant"]

    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if keep_build_dir:
        build_dir = out_dir / "build" / variant
        build_dir.mkdir(parents=True, exist_ok=True)
        run_cmake_only(target, spec["args"], build_dir)
        trace_data = load_trace_pickle(build_dir)
    else:
        with tempfile.TemporaryDirectory(
            prefix=f"kconfig-explorer-{variant}-"
        ) as tmp:
            run_cmake_only(target, spec["args"], tmp)
            trace_data = load_trace_pickle(tmp)

    snapshot = build_snapshot(variant, spec["board"], target, trace_data)

    out_path = out_dir / f"{board}.kconfig.json"
    with open(out_path, "w") as f:
        json.dump(snapshot, f, indent=2, sort_keys=True)

    return out_path
