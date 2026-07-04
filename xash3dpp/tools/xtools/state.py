"""Session-state layer: advisory checkpoints + derived ground truth.

Checkpoints (.agent-checkpoints.jsonl, gitignored) record INTENT — who was
doing what, when. They are never authoritative: `whereami()` always derives
ground truth from git, the implementation plan, the sync gates, and the OQ
crosswalk, and flags checkpoints as stale when the recorded HEAD no longer
matches. All document parsers here are deliberately tolerant (return empty
results on format misses, never raise) so a dormant repo can always be
diagnosed.
"""

from __future__ import annotations

import datetime as _dt
import importlib.util
import json
import os
import re
from pathlib import Path

from . import DOCS, REPO, SRC, XPP, venv_python
from .proc import run

CHECKPOINT_FILE = REPO / ".agent-checkpoints.jsonl"
MAX_RECENT = 5
CONCURRENT_WINDOW_H = 24

_PLAN = DOCS / "implementation-plan.md"
_REGISTER = DOCS / "design" / "decisions-architecture.md"

DOC_POINTERS = [
    ".github/WORKFLOW.md",
    ".github/AGENT-SETUP.md",
    "xash3dpp/docs/implementation-plan.md#tooling--automation-follow-ups",
]


# --------------------------------------------------------------------------- #
# git ground truth
# --------------------------------------------------------------------------- #

def _git(args: list[str]) -> tuple[int, list[str]]:
    rc, lines, _ = run(["git", "-C", str(REPO), *args])
    return rc, lines


def git_state() -> dict:
    def first(args, default=""):
        rc, lines = _git(args)
        return lines[0].strip() if rc == 0 and lines else default

    head = first(["rev-parse", "HEAD"])
    rc, status_lines = _git(["status", "--porcelain"])
    entries = [l for l in status_lines if l.strip()] if rc == 0 else []
    untracked = sum(1 for l in entries if l.startswith("??"))
    return {
        "branch": first(["rev-parse", "--abbrev-ref", "HEAD"]),
        "head": head,
        "head_short": head[:10],
        "head_subject": first(["log", "-1", "--format=%s"]),
        "dirty": bool(entries),
        "dirty_files": len(entries) - untracked,
        "untracked": untracked,
    }


# --------------------------------------------------------------------------- #
# checkpoint store (advisory)
# --------------------------------------------------------------------------- #

def read_checkpoints() -> tuple[list[dict], int]:
    if not CHECKPOINT_FILE.is_file():
        return [], 0
    entries: list[dict] = []
    malformed = 0
    for line in CHECKPOINT_FILE.read_text(encoding="utf-8",
                                          errors="replace").splitlines():
        if not line.strip():
            continue
        try:
            obj = json.loads(line)
            if isinstance(obj, dict):
                entries.append(obj)
            else:
                malformed += 1
        except ValueError:
            malformed += 1
    return entries, malformed


def append_checkpoint(chunk: str, step: str, note: str,
                      actor: str | None = None,
                      session: str | None = None) -> dict:
    now = _dt.datetime.now(_dt.timezone.utc)
    actor = actor or os.environ.get("XASH_CHECKPOINT_ACTOR", "agent")
    session = session or "%s-%s" % (actor, now.strftime("%Y-%m-%d"))
    g = git_state()
    entry = {
        "ts": now.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "actor": actor,
        "session": session,
        "branch": g["branch"],
        "head": g["head"],
        "chunk": chunk,
        "step": step,
        "note": note,
        "dirty": g["dirty"],
    }
    with CHECKPOINT_FILE.open("a", encoding="utf-8") as f:
        f.write(json.dumps(entry) + "\n")
    entries, _ = read_checkpoints()
    return {"entry": entry, "count": len(entries),
            "file": CHECKPOINT_FILE.name}


def _checkpoint_summary(head: str) -> dict:
    entries, malformed = read_checkpoints()
    recent = entries[-MAX_RECENT:][::-1]
    stale = bool(entries) and entries[-1].get("head") != head
    cutoff = _dt.datetime.now(_dt.timezone.utc) - _dt.timedelta(
        hours=CONCURRENT_WINDOW_H)
    sessions: list[str] = []
    for e in entries:
        try:
            ts = _dt.datetime.strptime(e.get("ts", ""), "%Y-%m-%dT%H:%M:%SZ")
            ts = ts.replace(tzinfo=_dt.timezone.utc)
        except ValueError:
            continue
        if ts >= cutoff and e.get("session") and e["session"] not in sessions:
            sessions.append(e["session"])
    return {
        "file": CHECKPOINT_FILE.name,
        "count": len(entries),
        "malformed": malformed,
        "recent": recent,
        "stale": stale,
        "concurrent_sessions": sessions if len(sessions) >= 2 else [],
    }


# --------------------------------------------------------------------------- #
# plan / register parsers (tolerant by design)
# --------------------------------------------------------------------------- #

_CHUNK_RX = re.compile(r"^### Chunk (\d+) — (.+?)\s*$", re.MULTILINE)


def plan_summary() -> dict:
    try:
        text = _PLAN.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {"updated": "", "chunks": [], "next_todo_chunk": None,
                "status_drift": ["implementation-plan.md unreadable"]}
    m = re.search(r"Updated:\s*([0-9-]+)", text)
    updated = m.group(1) if m else ""
    chunks = []
    matches = list(_CHUNK_RX.finditer(text))
    for i, cm in enumerate(matches):
        num, title = int(cm.group(1)), cm.group(2)
        if "✅ DONE" in title:
            status = "done"
        elif "tombstone" in title.lower():
            status = "tombstone"
        elif "IN PROGRESS" in title.upper():
            status = "in-progress"
        else:
            status = "todo"
        section_end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        section = text[cm.end():section_end]
        sub = None
        sm = re.search(r"\*\*Subsystems\*\*:\s*`?(\w+)`?", section)
        if sm:
            sub = sm.group(1)
        name = re.sub(r"\s*(✅ DONE|\*\(tombstone\)\*|\*\(.*?\)\*)\s*", " ",
                      title).strip().strip("—").strip()
        chunks.append({"num": num, "name": name, "status": status,
                       "subsystem": sub})
    next_todo = None
    for c in sorted(chunks, key=lambda c: c["num"]):
        if c["status"] in ("todo", "in-progress") and c["subsystem"]:
            boundary = DOCS / "boundaries" / ("%s-boundary.md" % c["subsystem"])
            src_dir = SRC / c["subsystem"]
            next_todo = {
                **c,
                "recon_done": boundary.is_file(),
                "scaffolded": src_dir.is_dir() and any(src_dir.rglob("*.cpp")),
            }
            break
    try:
        from .checks import status_check, status_table
        drift = status_check(status_table())
    except Exception:  # noqa: BLE001 — tolerant by design
        drift = ["status_table check failed to run"]
    return {"updated": updated, "chunks": chunks,
            "next_todo_chunk": next_todo, "status_drift": drift}


def blocking_oqs() -> list[dict]:
    try:
        text = _REGISTER.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    m = re.search(r"## 3a\..*?(?=\n## |\n_{10,})", text, re.DOTALL)
    if not m:
        return []
    out = []
    for line in m.group(0).splitlines():
        if not line.startswith("|") or line.startswith("| Doc") or \
                set(line.replace("|", "").strip()) <= {"-", " "}:
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 4:
            continue
        blocks = cells[3].replace("*", "").replace("`", "").strip().lower()
        if "scaffold" not in blocks:
            continue
        oq_cell = cells[1].replace("*", "").strip()
        om = re.match(r"(OQ-[\d…. ]+|[A-Z]+-OQ-\d+)\s*(?:\((.*)\))?", oq_cell)
        out.append({
            "doc": cells[0].replace("`", "").strip(),
            "oq": (om.group(1).strip() if om else oq_cell),
            "topic": (om.group(2) or "").strip() if om else "",
            "status": cells[2].replace("*", "").strip(),
            "blocks": blocks,
        })
    return out


# --------------------------------------------------------------------------- #
# doctor
# --------------------------------------------------------------------------- #

def doctor() -> dict:
    from . import vsenv
    checks: list[dict] = []
    warnings: list[str] = []

    def add(name, ok, detail, stale=None):
        c = {"name": name, "ok": ok, "detail": detail}
        if stale is not None:
            c["stale"] = stale
        checks.append(c)

    vp = venv_python()
    add("venv-python", vp.is_file(), str(vp))
    add("mcp-package", importlib.util.find_spec("mcp") is not None,
        "importable" if importlib.util.find_spec("mcp") else
        "not installed — pip install -r requirements.txt")
    rc, lines = _git(["--version"])
    add("git", rc == 0, lines[0].strip() if lines else "git not found")
    for name, resolver in (("cmake", vsenv.cmake_path),
                           ("ctest", vsenv.ctest_path),
                           ("vsdevcmd", vsenv.vsdevcmd_path),
                           ("clangd", vsenv.clangd_path)):
        try:
            add(name, True, str(resolver()))
        except FileNotFoundError as exc:
            add(name, False, str(exc))
    cache = XPP / "build" / "Debug" / "CMakeCache.txt"
    add("build-tree", cache.is_file(),
        "%s %s" % (cache.relative_to(REPO).as_posix(),
                   "present" if cache.is_file() else
                   "missing — run tools/build.py --configure"))
    db = XPP / "build" / "clangd" / "compile_commands.json"
    if db.is_file():
        newest = 0.0
        newest_name = ""
        for cml in XPP.rglob("CMakeLists.txt"):
            if "build" in cml.parts:
                continue
            mt = cml.stat().st_mtime
            if mt > newest:
                newest, newest_name = mt, cml.relative_to(REPO).as_posix()
        stale = db.stat().st_mtime < newest
        add("compile-db", True,
            "%s%s" % (db.relative_to(REPO).as_posix(),
                      " (older than %s)" % newest_name if stale else ""),
            stale=stale)
        if stale:
            warnings.append("compile-db is older than %s — run "
                            "tools/refresh_compile_db.py" % newest_name)
    else:
        add("compile-db", False,
            "missing — run tools/refresh_compile_db.py", stale=False)
    return {"ok": all(c["ok"] for c in checks), "warnings": warnings,
            "checks": checks}


# --------------------------------------------------------------------------- #
# routing + assembly
# --------------------------------------------------------------------------- #

def suggested_next(git: dict, plan: dict, gates: dict, oqs: list[dict],
                   cps: dict, doc: dict | None) -> dict:
    nxt = plan.get("next_todo_chunk") or {}
    last_note = ""
    if cps.get("recent"):
        e = cps["recent"][0]
        last_note = " (last checkpoint: %s/%s — %s)" % (
            e.get("chunk", "?"), e.get("step", "?"), e.get("note", ""))

    if doc is not None and not doc.get("ok", True):
        return {"rule": "doctor-failed",
                "action": "fix the environment before any work",
                "command": "see the failing doctor checks + the env-override "
                           "table in .github/AGENT-SETUP.md",
                "reason": "; ".join(c["detail"] for c in doc["checks"]
                                    if not c["ok"])[:300]}
    if git.get("dirty"):
        return {"rule": "dirty-tree",
                "action": "review or commit the working tree before new work",
                "command": "git status; commit per WORKFLOW.md commit discipline",
                "reason": "%d modified + %d untracked files"
                          % (git["dirty_files"], git["untracked"])}
    total = gates.get("sync_stage1", {}).get("findings", 0) + \
        gates.get("sync_stage2", {}).get("findings", 0)
    if total:
        return {"rule": "sync-red",
                "action": "repair the workflow surface",
                "command": r".venv\Scripts\python.exe xash3dpp\tools\workflow_sync.py --json",
                "reason": "%d sync findings" % total}
    if plan.get("status_drift"):
        return {"rule": "plan-drift",
                "action": "refresh the implementation-plan status table",
                "command": r".venv\Scripts\python.exe xash3dpp\tools\status_table.py --check --json",
                "reason": "; ".join(str(d) for d in plan["status_drift"])[:300]}
    if oqs and nxt and not nxt.get("scaffolded"):
        return {"rule": "blocking-oqs",
                "action": "decision session: resolve blocks-scaffold OQs "
                          "before scaffolding %s" % (nxt.get("subsystem") or "the next chunk"),
                "command": "read decisions-architecture.md §3a; decide %s"
                           % ", ".join("%s#%s" % (o["doc"], o["oq"]) for o in oqs),
                "reason": "%d open OQs block scaffold for chunk %s (%s)%s"
                          % (len(oqs), nxt.get("num"), nxt.get("subsystem"),
                             last_note)}
    if nxt and not nxt.get("recon_done"):
        return {"rule": "recon-missing",
                "action": "run the recon pass for the next chunk",
                "command": "analyse-subsystem %s" % nxt.get("subsystem"),
                "reason": "chunk %s (%s) has no boundary spec yet%s"
                          % (nxt.get("num"), nxt.get("subsystem"), last_note)}
    if nxt:
        if not nxt.get("scaffolded"):
            action, command = ("scaffold the next chunk",
                               "scaffold-subsystem %s" % nxt.get("subsystem"))
        else:
            action = "continue chunk %s (%s) per the WORKFLOW pipeline" % (
                nxt.get("num"), nxt.get("subsystem"))
            command = "plan-implementation %s / implement + write-unit-tests" \
                % nxt.get("subsystem")
        return {"rule": "next-chunk-step", "action": action,
                "command": command,
                "reason": "gates green; next chunk %s (%s)%s"
                          % (nxt.get("num"), nxt.get("subsystem"), last_note)}
    return {"rule": "next-chunk-step", "action": "all chunks done or plan "
            "unparseable — read the implementation plan",
            "command": "xash3dpp/docs/implementation-plan.md",
            "reason": "no next TODO chunk found%s" % last_note}


def whereami(doctor_requested: bool = False) -> dict:
    from .sync import workflow_sync
    git = git_state()
    plan = plan_summary()
    try:
        sync = workflow_sync(stage=2)
        gates = {
            "sync_stage1": {"findings": sync["counts"]["stage1"]},
            "sync_stage2": {"findings": sync["counts"]["stage2"]},
        }
    except Exception:  # noqa: BLE001 — tolerant by design
        gates = {"sync_stage1": {"findings": -1},
                 "sync_stage2": {"findings": -1}}
    oqs = blocking_oqs()
    cps = _checkpoint_summary(git["head"])
    doc = doctor() if doctor_requested else None
    out = {
        "git": git,
        "plan": plan,
        "gates": gates,
        "blocking_oqs": oqs,
        "checkpoints": cps,
        "suggested_next": suggested_next(git, plan, gates, oqs, cps, doc),
        "doc_pointers": DOC_POINTERS,
    }
    if doc is not None:
        out["doctor"] = doc
    return out
