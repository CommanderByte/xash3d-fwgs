#!/usr/bin/env python
"""Regenerate the per-subsystem status table from the tree.

--markdown prints the table for pasting; --check diffs against the status
table maintained in xash3dpp/docs/implementation-plan.md.
"""
import argparse
import sys

from xtools.checks import status_check, status_markdown, status_table
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--markdown", action="store_true")
    ap.add_argument("--check", action="store_true",
                    help="diff against implementation-plan.md")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = status_table()
        if args.check:
            data["drift"] = status_check(data)
            return not data["drift"], data
        return True, data

    def human(data):
        print(status_markdown(data))
        for d in data.get("drift", []):
            print("DRIFT: %s" % d)

    if args.markdown and not args.json:
        print(status_markdown(status_table()))
        return 0
    return cli_main("status_table", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
