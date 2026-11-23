#!/usr/bin/env python3

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Script to write the CI report as a single, self-contained HTML page.
#
# The page is made of ci_report.html, with ci_report.css and ci_report.js
# inlined, and the report data embedded as gzip-compressed, base64-encoded
# JSON (very repetitive: about 10x smaller), unpacked by the page itself. It
# is uploaded as a non-zipped artifact, that GitHub serves as a web page (see
# package_core.yml): it must not load anything from outside.
#
# Usage: ci_report_page.py <out.html> <build_info.json>
#                          [--sizes <dir>] [--deltas <dir>] [--size-deltas available|unavailable|none]
#  - build_info.json: data of the core artifacts, see ci_collect_data.py
#  - --sizes: folder of the arduino-*.json compilation reports, for the sketch sizes
#  - --deltas: folder of the same reports with the size deltas to the base
#    commit, see ci_calc_size_reports.py
#  - --size-deltas: whether the deltas were computed (default: none)
# The information on the CI run (repository, event, commit, branch or pull
# request, date) is taken from the GitHub Actions environment, see run_info().

import argparse
import base64
from datetime import datetime, timezone
import glob
import gzip
import json
import os
import re

from ci_shared import TestStatus

HERE = os.path.dirname(os.path.abspath(__file__))
PAYLOAD_VERSION = 1

# the GitHub emoji shortcodes of the markdown report, as Unicode
ICONS = {
    ":new_moon:": "🌑", ":green_circle:": "🟢", ":white_check_mark:": "✅",
    ":yellow_circle:": "🟡", ":no_entry_sign:": "🚫", ":heavy_check_mark:": "✔️",
    ":interrobang:": "⁉️", ":red_circle:": "🔴", ":x:": "❌", ":fire:": "🔥",
    ":label:": "🏷️", ":green_book:": "📗",
}

# sketch name in the delta reports, see ci_calc_size_reports.py
DELTA_NAME_RE = re.compile(r'(libraries|examples)/([^/]+)/(examples/|extras/)?(.*)')

SIZE_KEYS = {"flash": "flash", "RAM for global variables": "ram"}


def icon(shortcode):
    """Converts ':name:' or ':name:*' to its Unicode emoji, keeping the suffix."""
    for code, emoji in ICONS.items():
        if shortcode.startswith(code):
            return emoji + shortcode[len(code):]
    return shortcode


def delta_name(sketch):
    match = DELTA_NAME_RE.search(sketch)
    return f"{match.group(2)} {match.group(4)}" if match else sketch


def parse_fqbn(fqbn):
    """'arduino:<subarch>:<board>:link_mode=<mode>' -> (board, link_mode)"""
    parts = fqbn.split(":")
    board = parts[2] if len(parts) > 2 else fqbn
    options = dict(opt.split("=", 1) for opt in ":".join(parts[3:]).split(",") if "=" in opt)
    return board, options.get("link_mode", "")


def read_report(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def read_sizes(current_dir, deltas_dir):
    """
    Returns the size of each sketch, from the compilation reports in
    current_dir, with the delta to the base commit from the reports of the
    same name in deltas_dir, if any.
    """
    sizes = []
    for path in sorted(glob.glob(os.path.join(current_dir, "arduino-*.json"))):
        report = read_report(path)
        if not report or not report.get("boards"):
            continue
        deltas = {}
        if deltas_dir:
            delta_report = read_report(os.path.join(deltas_dir, os.path.basename(path)))
            for sketch in ((delta_report or {}).get("boards") or [{}])[0].get("sketches", []):
                deltas[sketch.get("name")] = {e["name"]: e.get("delta", {}).get("absolute")
                                              for e in sketch.get("sizes") or []}
        board_report = report["boards"][0]
        board, link_mode = parse_fqbn(board_report.get("board", ""))
        for sketch in board_report.get("sketches", []):
            if not sketch.get("sizes"):
                continue
            entry = {"board": board, "link_mode": link_mode, "sketch": sketch["name"]}
            sketch_deltas = deltas.get(delta_name(sketch["name"]), {})
            for size in sketch["sizes"]:
                key = SIZE_KEYS.get(size["name"])
                if not key:
                    continue
                delta = sketch_deltas.get(size["name"])
                entry[key] = {"used": size.get("current", {}).get("absolute"),
                              "max": size.get("maximum"),
                              "delta": delta if isinstance(delta, int) else None}
            sizes.append(entry)
    return sizes


def run_info(env, now):
    """
    Returns the information on the CI run from the GitHub Actions environment
    variables and event payload (only the date outside of GitHub Actions):
    repository, workflow run, event, actor, commit (head commit of a pull
    request), its title (first line of the pushed commit, or pull request
    title), branch, or pull request with its base branch.
    """
    event = {}
    try:
        with open(env.get("GITHUB_EVENT_PATH", "")) as f:
            event = json.load(f)
    except (OSError, ValueError):
        pass
    server = env.get("GITHUB_SERVER_URL", "https://github.com")
    repo = env.get("GITHUB_REPOSITORY")
    url = f"{server}/{repo}" if repo else None
    pr = event.get("pull_request") or {}
    sha = pr.get("head", {}).get("sha") or env.get("GITHUB_SHA")
    info = {
        "date": now.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "repository": repo,
        "repository_url": url,
        "workflow": env.get("GITHUB_WORKFLOW"),
        "run_number": env.get("GITHUB_RUN_NUMBER"),
        "run_attempt": env.get("GITHUB_RUN_ATTEMPT"),
        "event": env.get("GITHUB_EVENT_NAME"),
        "actor": env.get("GITHUB_TRIGGERING_ACTOR") or env.get("GITHUB_ACTOR"),
        "sha": sha,
        "commit_url": f"{url}/commit/{sha}" if url and sha else None,
    }
    if pr:
        info.update(pr_number=pr.get("number"), title=pr.get("title"), pr_url=pr.get("html_url"),
                    branch=pr.get("head", {}).get("label"), base=pr.get("base", {}).get("ref"))
    else:
        message = (event.get("head_commit") or {}).get("message") or ""
        branch = env.get("GITHUB_REF_NAME")
        info.update(title=message.split("\n")[0], branch=branch,
                    branch_url=f"{url}/tree/{branch}" if url and branch else None)
    return {k: v for k, v in info.items() if v}


def build_payload(build_info, sizes, size_deltas, info=None):
    """
    Returns the data embedded in the page, see the module comment.
    info: the information on the CI run, see run_info().
    """
    artifacts = list(build_info["artifacts"])
    boards = dict(build_info["all_board_data"])
    loaders = dict(build_info["board_loaders"])
    tests = list(build_info["all_tests"])

    return {
        "version": PAYLOAD_VERSION,
        "run": {
            "job_link_stem": build_info["job_link_stem"],
            "ci_run_passed": build_info["ci_run_passed"],
            "artifacts": artifacts,
        },
        "info": info or {},
        "statuses": [{"value": int(s), "name": s.name, "passed": s.passed,
                      "test_icon": icon(s.test_icon), "board_icon": icon(s.board_icon),
                      "legend": s.legend} for s in TestStatus],
        "icons": {"warnings": icon(":label:"), "no_warnings": icon(":green_book:")},
        "boards": boards,
        "regions_by_soc": build_info.get("regions_by_soc", {}),
        "loaders": loaders,
        "tests": tests,
        "sizes": sizes,
        "size_deltas": size_deltas,
    }


def read_asset(name):
    with open(os.path.join(HERE, name)) as f:
        return f.read()


def write_page(payload, out_path):
    """Writes the page: template with CSS, JS and the packed payload inlined."""
    data = base64.b64encode(gzip.compress(json.dumps(payload, separators=(",", ":")).encode(), 9)).decode()
    # '</' cannot appear in an inline script or style
    css = read_asset("ci_report.css").replace("</", "<\\/")
    js = read_asset("ci_report.js").replace("</", "<\\/")
    page = read_asset("ci_report.html")
    for placeholder, value in (("/*CSS*/", css), ("/*JS*/", js), ("/*DATA*/", data)):
        page = page.replace(placeholder, value, 1)
    with open(out_path, "w") as f:
        f.write(page)


def main():
    parser = argparse.ArgumentParser(description="Write the CI report page")
    parser.add_argument("out")
    parser.add_argument("build_info")
    parser.add_argument("--sizes")
    parser.add_argument("--deltas")
    parser.add_argument("--size-deltas", choices=["available", "unavailable", "none"], default="none")
    args = parser.parse_args()

    with open(args.build_info) as f:
        build_info = json.load(f)
    sizes = read_sizes(args.sizes, args.deltas) if args.sizes else []
    info = run_info(os.environ, datetime.now(timezone.utc))
    write_page(build_payload(build_info, sizes, args.size_deltas, info), args.out)


if __name__ == "__main__":
    main()
