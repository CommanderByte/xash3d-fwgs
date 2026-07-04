#!/usr/bin/env python
"""Change inventory since a base ref — the gate-agent briefing pack.

Default base: HEAD when the tree is dirty at a checkpointed commit (the
slice is the uncommitted work); else the newest checkpoint head that
differs from HEAD (the previous stable state), else HEAD~1. Spans
committed AND uncommitted tracked changes; untracked files listed
separately."""
import argparse
import sys

from xtools.report import cli_main, print_table
from xtools.state import slice_diff


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", default="",
                    help="git ref to diff against (default: HEAD when dirty "
                         "at a checkpointed commit, else last differing "
                         "checkpoint head, else HEAD~1)")
    ap.add_argument("--patch", action="store_true",
                    help="include the unified diff (capped)")
    ap.add_argument("--max-patch-lines", type=int, default=400)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = slice_diff(base=args.base, include_patch=args.patch,
                          max_patch_lines=args.max_patch_lines)
        return "error" not in data, data

    def human(data):
        print("base %s (%s) → %s  +%d/-%d in %d files, %d untracked" % (
            data["base"][:10], data["base_source"], data["head"],
            data["total_added"], data["total_deleted"],
            len(data["files"]), len(data["untracked"])))
        print_table(data["files"], ["status", "path", "added", "deleted"])
        for path in data["untracked"]:
            print("??  %s" % path)

    return cli_main("slice_diff", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
