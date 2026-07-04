#!/usr/bin/env python
"""Append an advisory checkpoint to .agent-checkpoints.jsonl (gitignored).

Record one at every commit, handoff, or interruption. Checkpoints are
intent notes for resumption — ground truth is always derived by whereami."""
import argparse
import sys

from xtools import state
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chunk", required=True,
                    help="chunk/topic label, e.g. '6-server' or 'workflow'")
    ap.add_argument("--step", required=True,
                    help="workflow step, e.g. 'pre-pr', 'sweep-module'")
    ap.add_argument("--note", required=True, help="one-line status/intent")
    ap.add_argument("--actor", default=None,
                    help="default: XASH_CHECKPOINT_ACTOR env or 'agent'")
    ap.add_argument("--session", default=None,
                    help="default: <actor>-<utc-date>")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = state.append_checkpoint(args.chunk, args.step, args.note,
                                       args.actor, args.session)
        return True, data

    def human(data):
        e = data["entry"]
        print("checkpoint #%d -> %s" % (data["count"], data["file"]))
        print("  %s [%s/%s] %s @ %s: %s/%s — %s" % (
            e["ts"], e["actor"], e["session"], e["branch"],
            e["head"][:10], e["chunk"], e["step"], e["note"]))

    return cli_main("checkpoint", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
