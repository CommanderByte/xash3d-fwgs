#!/usr/bin/env python
"""Lint markdown files with the repo's pymarkdownlnt config
(xash3dpp/.pymarkdown.json). Paths are repo-relative files or directories.

CLI twin of the MCP `markdown_lint` tool (MCP: xash-tools) — hooks and CI
can now run the same gate the agents use."""
import argparse
import sys

from xtools import mdlint
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("paths", nargs="+",
                    help="repo-relative markdown files or directories")
    ap.add_argument("--out", default=None,
                    help="write the envelope JSON to this file instead of "
                         "stdout (one-line summary still printed)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = mdlint.lint(args.paths)
        return data["exit_code"] == 0, data

    def human(data):
        print("%d issue(s)  [config %s]" % (data["issue_count"], data["config"]))
        for issue in data["issues"]:
            print("  " + issue)

    return cli_main("markdown_lint", run, args.json, human, out=args.out)


if __name__ == "__main__":
    sys.exit(main())
