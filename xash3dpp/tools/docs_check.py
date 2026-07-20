#!/usr/bin/env python
"""Documentation-drift scan: doc claims about code must stay checkable.

Two families (--checks all | anchors | claims):

  anchors  every backticked `file.cpp:123` citation must resolve and must
           still point at the text recorded in docs/.doc-anchors.json.
           --bless records the current text; --repair rewrites line numbers
           whose recorded text merely MOVED (the common case, fixed for free).

  claims   `<!-- verify: <predicate> -->` next to a binding assertion.
           Vocabulary: callers(sym) / symbol-exists(sym) / impls(IFace) /
           compliance-rule-exists(id) / census(subsystem, key), each
           optionally compared with ==, >=, <=, >, < to an integer
           (default `>= 1`).

Exempt one anchor with `<!-- verify-skip: reason -->` on it or the line above.

Rationale: the 2026-07-20 modernization audit found ~17 doc claims that were
false, none of them catchable, because prose assertions are not checkable.
This makes BINDING claims checkable; unanchored prose is advisory by design.
"""
import argparse
import sys

from xtools import docs
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--checks", default="all",
                    choices=["all", "anchors", "claims"])
    ap.add_argument("--bless", action="store_true",
                    help="(re)record every resolvable anchor's current text")
    ap.add_argument("--repair", action="store_true",
                    help="rewrite doc line numbers whose recorded text moved")
    ap.add_argument("--out", default=None,
                    help="write the envelope JSON to this file instead of "
                         "stdout (one-line summary still printed)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = docs.scan(checks=args.checks, bless=args.bless,
                         repair=args.repair)
        return not data["findings"], data

    def human(data):
        c = data["counts"]
        print("%d doc(s): %d anchor(s) (%d skipped), %d claim(s)"
              % (data["docs"], c["anchors"], c["skipped"], c["claims"]))
        if c["blessed"]:
            print("blessed %d anchor(s) -> %s" % (c["blessed"], data["lockfile"]))
        for r in data["repaired"]:
            print("  repaired %s:%d  line %d -> %d"
                  % (r["doc"], r["line"], r["from"], r["to"]))
        for f in data["findings"]:
            print("  [%s] %s:%d  %s"
                  % (f["kind"], f["doc"], f["line"], f["detail"]))
        for n in data["notes"]:
            print("  note: %s:%d  %s" % (n["doc"], n["line"], n["detail"]))

    return cli_main("docs_check", run, args.json, human, out=args.out)


if __name__ == "__main__":
    sys.exit(main())
