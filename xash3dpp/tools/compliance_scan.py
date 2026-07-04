#!/usr/bin/env python
"""Mechanical convention scan ([M] checks from the reviewer charter).

Check sets: all | prepr (pre-pr Phase 2) | detail (detail-audit) | id,id,...
Findings marked candidate-* are heuristics needing agent judgment.

Scopes: a subsystem name / 'all', an explicit --files list, or --slice
(the current change set from slice_diff's default base) — slices cross
subsystem boundaries; gates should scan what actually changed.
"""
import argparse
import sys

from xtools.checks import compliance_scan
from xtools.report import cli_main, print_table


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("subsystem", nargs="?", default="",
                    help="subsystem name or 'all' (omit with --files/--slice)")
    ap.add_argument("--files", default="",
                    help="comma-separated repo-relative paths (overrides "
                         "subsystem discovery)")
    ap.add_argument("--slice", action="store_true",
                    help="scan the current slice: changed + untracked files "
                         "from slice_diff's default base")
    ap.add_argument("--checks", default="all",
                    help="all | prepr | detail | comma-list of check ids")
    ap.add_argument("--min-severity", default="note",
                    choices=["note", "warning", "blocker"])
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        # Scope mistakes are execution errors (exit 2 via the cli_main
        # exception path), never "findings present".
        files = None
        if args.slice:
            from xtools.state import slice_diff
            diff = slice_diff()
            if "error" in diff:
                raise RuntimeError(diff["error"])
            files = [f["path"] for f in diff["files"]] + diff["untracked"]
        elif args.files:
            files = [f for f in args.files.split(",") if f.strip()]
        elif not args.subsystem:
            raise RuntimeError("give a subsystem, --files, or --slice")

        data = compliance_scan(args.subsystem or None, checks=args.checks,
                               min_severity=args.min_severity, files=files)
        hard = data["counts"]["blocker"] + data["counts"]["warning"] \
            + data["counts"]["note"] - data["counts"]["candidate"]
        return hard == 0 and not data["violations"], data

    def human(data):
        print("scanned %d files in %s (checks=%s)" % (
            data["files_scanned"], ", ".join(data["subsystems"]), data["checks"]))
        if data.get("files_ignored"):
            print("ignored %d non-scannable input(s): %s" % (
                len(data["files_ignored"]),
                ", ".join(data["files_ignored"][:10])))
        print_table(data["violations"],
                    ["severity", "check", "file", "line", "excerpt"])
        print("counts: %s" % data["counts"])
        print("judgment checks NOT run (reviewer [J] set): %d areas"
              % len(data["judgment_checks_not_run"]))

    return cli_main("compliance_scan", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
