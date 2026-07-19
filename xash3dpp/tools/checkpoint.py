#!/usr/bin/env python
"""Append an advisory checkpoint to .agent-checkpoints.jsonl (gitignored).

Record one at every commit, handoff, or interruption. Checkpoints are
intent notes for resumption — ground truth is always derived by whereami.

Search mode (T7): `--grep <regex>` and/or `--filter-chunk <name>` read the
log instead of appending (`--limit N` newest matches, default 20)."""
import argparse
import sys

from xtools import state
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chunk", default=None,
                    help="chunk/topic label, e.g. '6-server' or 'workflow' "
                         "(required in append mode)")
    ap.add_argument("--step", default=None,
                    help="workflow step, e.g. 'pre-pr', 'sweep-module' "
                         "(required in append mode)")
    ap.add_argument("--note", default=None,
                    help="one-line status/intent (required in append mode)")
    ap.add_argument("--actor", default=None,
                    help="default: XASH_CHECKPOINT_ACTOR env or 'agent'")
    ap.add_argument("--session", default=None,
                    help="default: <actor>-<utc-date>")
    ap.add_argument("--grep", default=None,
                    help="SEARCH mode: case-insensitive regex over "
                         "chunk/step/note/actor/session/branch")
    ap.add_argument("--filter-chunk", default=None,
                    help="SEARCH mode: exact chunk match")
    ap.add_argument("--limit", type=int, default=20,
                    help="search mode: newest N matches (default 20; <=0 all)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    search_mode = args.grep is not None or args.filter_chunk is not None
    if not search_mode and not (args.chunk and args.step and args.note):
        ap.error("append mode requires --chunk, --step and --note "
                 "(use --grep/--filter-chunk to search instead)")

    def run():
        if search_mode:
            return True, state.search_checkpoints(
                args.grep, args.filter_chunk, args.limit)
        data = state.append_checkpoint(args.chunk, args.step, args.note,
                                       args.actor, args.session)
        return True, data

    def human(data):
        if search_mode:
            print("%d/%d checkpoint(s) matched (%s)" % (
                data["matched"], data["count_total"], data["file"]))
            for e in data["entries"]:
                print("  %s [%s] %s/%s @ %s — %s" % (
                    e.get("ts", "?"), e.get("actor", "?"), e.get("chunk", "?"),
                    e.get("step", "?"), str(e.get("head", ""))[:10],
                    e.get("note", "")[:160]))
            return
        e = data["entry"]
        print("checkpoint #%d -> %s" % (data["count"], data["file"]))
        print("  %s [%s/%s] %s @ %s: %s/%s — %s" % (
            e["ts"], e["actor"], e["session"], e["branch"],
            e["head"][:10], e["chunk"], e["step"], e["note"]))

    return cli_main("checkpoint", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
