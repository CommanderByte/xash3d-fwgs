#!/usr/bin/env python
"""Mechanical convention scan ([M] checks from the reviewer charter).

Check sets: all | prepr (pre-pr Phase 2) | detail (detail-audit) | id,id,...
Findings marked candidate-* are heuristics needing agent judgment.

Scopes: a subsystem name / 'all', an explicit --files list, or --slice
(the current change set from slice_diff's default base) — slices cross
subsystem boundaries; gates should scan what actually changed.
"""
import argparse
import sys
from pathlib import Path

from xtools.checks import _sub_of_path, compliance_scan
from xtools.report import cli_main, print_table, write_json_file


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("subsystem", nargs="?", default="",
                    help="subsystem name or 'all' (omit with --files/--slice)")
    ap.add_argument("--files", default="",
                    help="comma-separated repo-relative paths (overrides "
                         "subsystem discovery)")
    ap.add_argument("--slice", action="store_true",
                    help="scan the current slice: changed + untracked files "
                         "from slice_diff's default base")
    ap.add_argument("--baseline", action="store_true",
                    help="with the slice change set, keep only findings the "
                         "change introduced (drop pre-existing in touched files)")
    ap.add_argument("--checks", default="all",
                    help="all | prepr | detail | comma-list of check ids")
    ap.add_argument("--min-severity", default="note",
                    choices=["note", "warning", "blocker"])
    ap.add_argument("--out", default=None,
                    help="write the envelope JSON to this file instead of "
                         "stdout (one-line summary still printed)")
    ap.add_argument("--split-by-subsystem", default=None, metavar="DIR",
                    help="additionally write DIR/<subsystem>.json per "
                         "subsystem (violations grouped by finding path; "
                         "annotation-coverage splits the coverage map)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        # Scope mistakes are execution errors (exit 2 via the cli_main
        # exception path), never "findings present".
        files = None
        baseline_base = ""
        if args.slice or args.baseline:
            from xtools.state import slice_diff
            diff = slice_diff()
            if "error" in diff:
                raise RuntimeError(diff["error"])
            files = [f["path"] for f in diff["files"]] + diff["untracked"]
            if args.baseline:
                baseline_base = diff["base"]
        elif args.files:
            files = [f for f in args.files.split(",") if f.strip()]
        elif not args.subsystem:
            raise RuntimeError("give a subsystem, --files, or --slice")

        data = compliance_scan(args.subsystem or None, checks=args.checks,
                               min_severity=args.min_severity, files=files,
                               baseline_base=baseline_base)
        if args.split_by_subsystem:
            split_dir = Path(args.split_by_subsystem)
            written = {}
            if "coverage" in data:
                for sub, cov in data["coverage"].items():
                    p = split_dir / ("%s.json" % sub)
                    write_json_file(p, {"subsystem": sub, "coverage": cov})
                    written[sub] = str(p)
            else:
                groups: dict[str, list] = {}
                for v in data["violations"]:
                    groups.setdefault(
                        _sub_of_path(Path(v["file"])) or "_other", []).append(v)
                for sub, viols in sorted(groups.items()):
                    p = split_dir / ("%s.json" % sub)
                    write_json_file(p, {"subsystem": sub, "violations": viols})
                    written[sub] = str(p)
            data["split_written"] = written
        if args.checks == "annotation-coverage":
            # Coverage payload has no counts/violations envelope; clean =
            # every axis at 100% (6B S2/S3 CLI crash fix — the MCP path
            # never hit this).
            clean = all(
                slot["coverage_pct"] >= 100.0
                for cov in data["coverage"].values()
                for slot in cov.values() if isinstance(slot, dict))
            return clean, data
        hard = data["counts"]["blocker"] + data["counts"]["warning"] \
            + data["counts"]["note"] - data["counts"]["candidate"]
        return hard == 0 and not data["violations"], data

    def human(data):
        if "coverage" in data:
            for sub, cov in data["coverage"].items():
                print("%s annotation coverage:" % sub)
                for axis, slot in cov.items():
                    if isinstance(slot, dict):
                        print("  %-13s required=%-3d annotated=%-3d exempt=%-3d %.1f%%"
                              % (axis, slot["required"], slot["annotated"],
                                 slot["exempt"], slot["coverage_pct"]))
            return
        print("scanned %d files in %s (checks=%s)" % (
            data["files_scanned"], ", ".join(data["subsystems"]), data["checks"]))
        if data.get("baseline_base"):
            print("baseline vs %s: %d pre-existing finding(s) suppressed" % (
                data["baseline_base"][:12], data["baseline_suppressed"]))
        if data.get("files_ignored"):
            print("ignored %d non-scannable input(s): %s" % (
                len(data["files_ignored"]),
                ", ".join(data["files_ignored"][:10])))
        print_table(data["violations"],
                    ["severity", "check", "file", "line", "excerpt"])
        print("counts: %s" % data["counts"])
        print("judgment checks NOT run (reviewer [J] set): %d areas"
              % len(data["judgment_checks_not_run"]))

    return cli_main("compliance_scan", run, args.json, human, out=args.out)


if __name__ == "__main__":
    sys.exit(main())
