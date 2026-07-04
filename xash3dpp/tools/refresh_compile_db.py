#!/usr/bin/env python
"""Regenerate build/clangd/compile_commands.json (cpp-lsp / clangd database).

Wraps: VsDevCmd -arch=x64 && cmake --preset clangd   (from xash3dpp/)
"""
import argparse
import sys

from xtools.buildtools import refresh_compile_db
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = refresh_compile_db()
        return data["exit_code"] == 0 and data["entry_count"] > 0, data

    def human(data):
        print("%s: %d entries (refreshed=%s)" % (
            data["compile_commands"], data["entry_count"], data["refreshed"]))

    return cli_main("refresh_compile_db", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
