#!/usr/bin/env python
"""Subsystem dependency edges from cross-namespace references and the
InitParams inventory. Feeds the dependency-graph prompt."""
import argparse
import sys

from xtools.checks import dep_scan
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = dep_scan()
        return not data["cycles"], data

    def human(data):
        for e in data["edges"]:
            print(e)
        print("%d InitParams structs; cycles: %s" % (
            len(data["init_params"]), data["cycles"] or "none"))

    return cli_main("dep_scan", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
