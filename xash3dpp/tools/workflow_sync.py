#!/usr/bin/env python
"""Drift checker for the agent-workflow surface (.github originals vs
adapters, model dialects, twin entry files, MCP registrations, doc counters).

--stage 1  : tooling-session invariants only (Session 1 gate)
--stage 2  : everything (final gate; default)
"""
import argparse
import sys

from xtools.report import cli_main, print_table
from xtools.sync import workflow_sync


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--stage", type=int, default=2, choices=[1, 2])
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = workflow_sync(stage=args.stage)
        return not data["findings"], data

    def human(data):
        print_table(data["findings"], ["stage", "invariant", "detail"])
        print("stage-1 findings: %d, stage-2 findings: %d (checked at stage %d)"
              % (data["counts"]["stage1"], data["counts"]["stage2"], data["stage"]))

    return cli_main("workflow_sync", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
