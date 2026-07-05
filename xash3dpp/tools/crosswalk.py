#!/usr/bin/env python
"""Resolve a legacy engine symbol or file:line to its xash3dpp C++ port."""
import argparse
import sys

from xtools.crosswalk import crosswalk
from xtools.report import cli_main, print_table


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("query", nargs="?", default="",
                    help="legacy symbol (SV_Multicast) or file.c:line")
    ap.add_argument("--kind", choices=["auto", "symbol", "fileline"],
                    default="auto")
    ap.add_argument("--missing", action="store_true",
                    help="list deep-dive-documented symbols with no code port")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = crosswalk(args.query, kind=args.kind, missing=args.missing)
        clean = True if args.missing else data["count"] > 0
        return clean, data

    def human(data):
        if "missing" in data:
            print("%d documented-but-unported symbol(s)" % data["count"])
            print_table(data["missing"],
                        ["legacy_symbol", "legacy_file", "legacy_line",
                         "cpp_file"])
            return
        print("%d match(es) for %r" % (data["count"], data["query"]))
        if data.get("note"):
            print(data["note"])
        print_table(data["matches"],
                    ["legacy_symbol", "legacy_file", "legacy_line", "cpp_file",
                     "cpp_symbol", "confidence", "source"])

    return cli_main("crosswalk", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
