#!/usr/bin/env python
"""Build xash3dpp via the VS2022-bundled cmake (preset-driven)."""
import argparse
import sys

from xtools.buildtools import build
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--preset", default="debug")
    ap.add_argument("--arch", default="x64", choices=["x64", "x86"],
                    help="target width: x64 (default) or x86 (32-bit, for "
                         "the retail GoldSrc dlls/hl.dll)")
    ap.add_argument("--configure", action="store_true",
                    help="run the configure preset first")
    ap.add_argument("--target", default=None)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = build(preset=args.preset, configure=args.configure,
                     target=args.target, arch=args.arch)
        return data["exit_code"] == 0, data

    def human(data):
        print("preset=%s arch=%s exit=%d errors=%d warnings=%d (%.1fs)" % (
            data["preset"], data["arch"], data["exit_code"],
            data["error_count"], data["warning_count"], data["duration_s"]))
        for e in data["errors"][:10]:
            print("  %s(%s): %s %s" % (e["file"], e["line"], e["code"], e["text"]))

    return cli_main("build", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
