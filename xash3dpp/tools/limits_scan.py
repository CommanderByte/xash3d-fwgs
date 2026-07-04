#!/usr/bin/env python
"""limits.hpp audit: parse all XASH_LIMIT_* blocks, find magic-number
candidates in scope, detect dead limits and shadow literals."""
import argparse
import sys

from xtools.checks import limits_scan
from xtools.report import cli_main, print_table


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("subsystem", nargs="?", default=None,
                    help="optional subsystem scope (default: all)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = limits_scan(args.subsystem)
        return not (data["magic"] or data["shadow"] or data["unused"]), data

    def human(data):
        print("%d limits defined; %d unused; %d magic candidates; %d shadows"
              % (data["limits_total"], len(data["unused"]),
                 len(data["magic"]), len(data["shadow"])))
        if data["unused"]:
            print("dead limits: %s" % ", ".join(data["unused"]))
        print_table(data["magic"][:40], ["file", "line", "kind", "literal", "context"])
        print_table(data["shadow"][:40], ["file", "line", "literal", "matches_limit"])

    return cli_main("limits_scan", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
