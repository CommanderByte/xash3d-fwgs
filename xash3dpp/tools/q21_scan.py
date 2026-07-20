#!/usr/bin/env python
"""Q-21 axis-coverage scan: every boundary doc's 'Extension axes (Q-21)'
section must state a verdict for every G-*/P-* axis CURRENTLY listed in
docs/design/extension-goals.md (the set is additive — this catches the
drift class that produced 56 missing rows in the 2026-07 audit).

Mechanical pre-pass for the extension-door-auditor agent / the
audit-extension-doors prompt (MCP: xash-tools `q21_scan`); the
claims-vs-code judgment stays with the agent."""
import argparse
import sys

from xtools import q21
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=None,
                    help="write the envelope JSON to this file instead of "
                         "stdout (one-line summary still printed)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = q21.scan()
        return not data["findings"], data

    def human(data):
        s = data["summary"]
        print("axes: %s" % ", ".join(data["axes"]))
        print("%d/%d planned subsystem(s) have a boundary spec; "
              "%d of those clean; %d missing axis row(s), %d unknown axis id(s)"
              % (s["docs"], s["planned"], s["docs_clean"],
                 s["missing_total"], s["unknown_total"]))
        for f in data["findings"]:
            print("  [%s] %s" % (f["kind"], f["detail"]))
        for pnd in data.get("pending_specs", []):
            print("  pending: %s owes a spec at chunk start (%s not started)"
                  % (pnd["subsystem"], "/".join(pnd["chunks"]) or "unscheduled"))
        for sub, b in data["boundaries"].items():
            if b["unknown_axes"]:
                print("  note: %s names unknown axes %s (not in "
                      "extension-goals.md)" % (sub, b["unknown_axes"]))

    return cli_main("q21_scan", run, args.json, human, out=args.out)


if __name__ == "__main__":
    sys.exit(main())
