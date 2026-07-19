#!/usr/bin/env python
"""Scan a subsystem for TODO/stub markers and classify tests live vs stub."""
import argparse
import sys

from xtools.checks import stub_scan
from xtools.report import cli_main, print_table


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("subsystem")
    ap.add_argument("--delta", action="store_true",
                    help="add per-tag net marker change vs HEAD~1")
    ap.add_argument("--out", default=None,
                    help="write the envelope JSON to this file instead of "
                         "stdout (one-line summary still printed)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = stub_scan(args.subsystem, delta=args.delta)
        return data["todo_count"] == 0 and data["tests"]["stub"] == 0, data

    def human(data):
        print("%d TODO/stub markers; tests: %d live / %d stub of %d files" % (
            data["todo_count"], data["tests"]["live"], data["tests"]["stub"],
            data["tests"]["files"]))
        if data.get("by_tag"):
            print("by tag: " + ", ".join(
                "%s=%d" % (t, n) for t, n in data["by_tag"].items()))
        if data.get("delta_by_tag"):
            print("delta vs HEAD~1: " + ", ".join(
                "%s%+d" % (t, d) for t, d in data["delta_by_tag"].items()))
        print_table(data["stubs"], ["file", "line", "symbol", "marker"])

    return cli_main("stub_scan", run, args.json, human, out=args.out)


if __name__ == "__main__":
    sys.exit(main())
