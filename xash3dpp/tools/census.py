#!/usr/bin/env python
"""Per-subsystem ground-truth census: src TU count, assert_thread_role call
sites/files, compliance-allow tallies by rule, stub markers, test liveness.

The numbers boundary/threading docs keep quoting (and hand-counting, and
getting stale — the 2026-07 audit's doc-accuracy findings were mostly
numbers). Doc refreshes paste from this; audits diff quoted-vs-actual.
Informational — always exits 0. (MCP: xash-tools `census`.)"""
import argparse
import sys
from pathlib import Path

from xtools.checks import census
from xtools.report import cli_main, write_json_file


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("subsystem", nargs="?", default="",
                    help="subsystem name or 'all' (default all)")
    ap.add_argument("--out", default=None,
                    help="write the envelope JSON to this file instead of "
                         "stdout (one-line summary still printed)")
    ap.add_argument("--split-by-subsystem", default=None, metavar="DIR",
                    help="additionally write DIR/<subsystem>.json per "
                         "subsystem")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = census(args.subsystem or None)
        if args.split_by_subsystem:
            split_dir = Path(args.split_by_subsystem)
            written = {}
            for sub, entry in data["subsystems"].items():
                p = split_dir / ("%s.json" % sub)
                write_json_file(p, {"subsystem": sub, "census": entry})
                written[sub] = str(p)
            data["split_written"] = written
        return True, data

    def human(data):
        for sub, e in data["subsystems"].items():
            ta = e["assert_thread_role"]
            print("%-12s tu=%-3d asserts=%d/%d files  allows=%-3d "
                  "stubs=%-4d tests=%d (%d live)" % (
                      sub, e["src_tu_count"], ta["sites"], len(ta["files"]),
                      sum(e["compliance_allows_by_rule"].values()),
                      e["stub_markers"]["total"],
                      e["tests"]["files"], e["tests"]["live"]))
        t = data["totals"]
        print("totals: tu=%d asserts=%d allows=%d stubs=%d tests=%d "
              "(%d live)" % (t["src_tu_count"], t["assert_sites"],
                             t["allows"], t["stub_markers"],
                             t["test_files"], t["live_tests"]))

    return cli_main("census", run, args.json, human, out=args.out)


if __name__ == "__main__":
    sys.exit(main())
