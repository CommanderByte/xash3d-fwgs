#!/usr/bin/env python
"""Ground-truth session brief: git state, plan/chunk status, sync gates,
blocking OQs, recent checkpoints, and a suggested next action.

Run this at the start of every session (and after any dormancy).
--doctor adds environment checks (venv, VS tooling, build tree,
compile-DB freshness)."""
import argparse
import sys

from xtools import state
from xtools.report import cli_main


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--doctor", action="store_true",
                    help="add environment health checks")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    def run():
        data = state.whereami(doctor_requested=args.doctor)
        clean = data["doctor"]["ok"] if args.doctor else True
        return clean, data

    def human(data):
        g = data["git"]
        print("branch %s @ %s — %s%s" % (
            g["branch"], g["head_short"], g["head_subject"],
            " [DIRTY: %d+%d]" % (g["dirty_files"], g["untracked"])
            if g["dirty"] else ""))
        nxt = data["plan"].get("next_todo_chunk")
        if nxt:
            print("next chunk: %s (%s) — recon_done=%s scaffolded=%s" % (
                nxt["num"], nxt["subsystem"], nxt["recon_done"],
                nxt["scaffolded"]))
        print("gates: sync stage1=%s stage2=%s; blocking OQs: %s" % (
            data["gates"]["sync_stage1"]["findings"],
            data["gates"]["sync_stage2"]["findings"],
            ", ".join("%s#%s" % (o["doc"], o["oq"])
                      for o in data["blocking_oqs"]) or "none"))
        cps = data["checkpoints"]
        if cps["recent"]:
            e = cps["recent"][0]
            print("last checkpoint: %s %s/%s — %s%s" % (
                e.get("ts"), e.get("chunk"), e.get("step"), e.get("note"),
                " [STALE]" if cps["stale"] else ""))
        if cps["concurrent_sessions"]:
            print("WARNING: concurrent sessions in last 24h: %s"
                  % ", ".join(cps["concurrent_sessions"]))
        s = data["suggested_next"]
        print("next [%s]: %s\n  -> %s\n  (%s)" % (
            s["rule"], s["action"], s["command"], s["reason"]))
        for c in data.get("doctor", {}).get("checks", []):
            mark = "ok " if c["ok"] else "FAIL"
            print("doctor %s %-12s %s" % (mark, c["name"], c["detail"]))
        for w in data.get("doctor", {}).get("warnings", []):
            print("doctor WARN %s" % w)

    return cli_main("whereami", run, args.json, human)


if __name__ == "__main__":
    sys.exit(main())
