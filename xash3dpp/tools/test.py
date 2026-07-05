#!/usr/bin/env python
"""Run the xash3dpp ctest suite (preset-driven)."""
import argparse
import sys

from xtools.buildtools import test
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-R", "--filter", dest="filter", default="",
                    help="ctest -R regex")
    ap.add_argument("--preset", default="debug")
    ap.add_argument("--arch", default="x64", choices=["x64", "x86"],
                    help="target width: x64 (default) or x86 (32-bit suite)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = test(filter_regex=args.filter, preset=args.preset,
                    arch=args.arch)
        return data["exit_code"] == 0 and data["failed"] == 0, data

    def human(data):
        print("[%s] %d/%d passed, %d failed, %d skipped (%.1fs)" % (
            data["arch"], data["passed"], data["total"], data["failed"],
            data["skipped"], data["duration_s"]))
        for t in data["failed_tests"]:
            print("  FAILED %s (%s)" % (t["name"], t["reason"]))

    return cli_main("test", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
