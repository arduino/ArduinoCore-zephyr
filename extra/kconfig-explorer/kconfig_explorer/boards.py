# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

"""Board/target/variant resolution, mirroring extra/build.sh and
extra/build_all.sh so this tool always targets the same boards with the
same arguments as a real build."""

import json
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
GET_BOARD_DETAILS = REPO_ROOT / "extra" / "get_board_details.sh"
GET_VARIANT_NAME = REPO_ROOT / "extra" / "get_variant_name.sh"


class BoardResolutionError(Exception):
    pass


def get_board_details():
    """Returns the list of board entries from extra/get_board_details.sh,
    one dict per board defined in boards.txt (name, board, variant, target,
    args, hals, artifact, subarch, upload_offset)."""

    result = subprocess.run(
        [str(GET_BOARD_DETAILS)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise BoardResolutionError(
            f"get_board_details.sh failed:\n{result.stderr}"
        )
    return json.loads(result.stdout)


def resolve_board(name, board_list):
    """Resolves 'name' (a board or variant name) against known boards.txt
    entries: if it matches, use those target/args (same as extra/build.sh);
    otherwise return failure."""

    for entry in board_list:
        if entry["board"] == name or entry.get("variant") == name:
            return {
                "board": entry["board"],
                "variant": entry["variant"],
                "target": entry["target"],
                "args": entry.get("args") or "",
            }

    raise BoardResolutionError(
        f"failed to resolve board name for target {name!r}"
    )
