#!/usr/bin/env python
"""The 9-section finish-subsystem done checklist as data.

Single source of truth for both the finish-subsystem prompt and pre-pr
Phase 1. Items report pass | fail | needs-judgment (the consuming agent
resolves the judgment items).
"""
import argparse
import sys

from xtools.checks import finish_check
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("subsystem")
    ap.add_argument("--run-tests", action="store_true",
                    help="also run ctest -R test_<subsystem>")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = finish_check(args.subsystem, run_tests=args.run_tests)
        clean = all(i["status"] == "pass" for i in data["items"])
        return clean, data

    def human(data):
        for i in data["items"]:
            mark = {"pass": "[x]", "fail": "[ ]", "needs-judgment": "[?]"}[i["status"]]
            print("%s %d. %s — %s" % (mark, i["id"], i["name"],
                                      "; ".join(str(e) for e in i["evidence"])))
        print(data["summary"])

    return cli_main("finish_check", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
