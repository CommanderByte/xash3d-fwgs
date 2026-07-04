#!/usr/bin/env python
"""Mechanical convention scan ([M] checks from the reviewer charter).

Check sets: all | prepr (pre-pr Phase 2) | detail (detail-audit) | id,id,...
Findings marked candidate-* are heuristics needing agent judgment.
"""
import argparse
import sys

from xtools.checks import compliance_scan
from xtools.report import cli_main, print_table


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("subsystem", help="subsystem name or 'all'")
    ap.add_argument("--checks", default="all",
                    help="all | prepr | detail | comma-list of check ids")
    ap.add_argument("--min-severity", default="note",
                    choices=["note", "warning", "blocker"])
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = compliance_scan(args.subsystem, checks=args.checks,
                               min_severity=args.min_severity)
        hard = data["counts"]["blocker"] + data["counts"]["warning"] \
            + data["counts"]["note"] - data["counts"]["candidate"]
        return hard == 0 and not data["violations"], data

    def human(data):
        print("scanned %d files in %s (checks=%s)" % (
            data["files_scanned"], ", ".join(data["subsystems"]), data["checks"]))
        print_table(data["violations"],
                    ["severity", "check", "file", "line", "excerpt"])
        print("counts: %s" % data["counts"])
        print("judgment checks NOT run (reviewer [J] set): %d areas"
              % len(data["judgment_checks_not_run"]))

    return cli_main("compliance_scan", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
