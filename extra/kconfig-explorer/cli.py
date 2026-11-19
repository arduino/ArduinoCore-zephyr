#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

"""CLI to generate per-variant Kconfig state snapshots for the loader app.

Usage:
    cli.py list
    cli.py generate <board_or_variant> [<board_or_variant> ...] --out <dir>
    cli.py generate --all --out <dir> [--keep-build-dir]
"""

import argparse
import sys

from kconfig_explorer import boards, generate


def cmd_list(_args):
    board_list = boards.get_board_details()
    for entry in sorted(board_list, key=lambda e: e["board"]):
        print(f"{entry['board']}\t{entry['target']}\t{entry['variant']}")
    return 0


def cmd_generate(args):
    board_list = boards.get_board_details()
    targets = (
        [entry["board"] for entry in board_list] if args.all else args.names
    )

    failures = []
    for name in targets:
        try:
            out_path = generate.generate_variant(
                name, board_list, args.out, keep_build_dir=args.keep_build_dir
            )
            print(f"OK    {name} -> {out_path}")
        except Exception as exc:
            print(f"FAIL  {name}: {exc}", file=sys.stderr)
            failures.append(name)

    if len(targets) > 1:
        ok = len(targets) - len(failures)
        print(f"\n{ok}/{len(targets)} succeeded")

    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(prog="kconfig-explorer")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list", help="list boards from get_board_details.sh")

    generate_parser = subparsers.add_parser(
        "generate", help="generate <variant>.kconfig.json snapshot(s)"
    )
    generate_parser.add_argument(
        "names",
        nargs="*",
        metavar="BOARD_OR_VARIANT",
        help="one or more board names (from boards.txt) or variant names; "
        "ignored if --all is given",
    )
    generate_parser.add_argument(
        "--all", action="store_true", help="generate every board"
    )
    generate_parser.add_argument(
        "--out", required=True, help="output directory for snapshot files"
    )
    generate_parser.add_argument(
        "--keep-build-dir",
        action="store_true",
        help="keep the west build dir at <out>/build/<variant> instead of "
        "using a temp dir",
    )

    args = parser.parse_args()

    if args.command == "generate":
        if args.all and args.names:
            parser.error("cannot combine --all with explicit board/variant names")
        if not args.all and not args.names:
            parser.error("give at least one board/variant name, or use --all")

    try:
        if args.command == "list":
            return cmd_list(args)
        if args.command == "generate":
            return cmd_generate(args)
        return 1
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
