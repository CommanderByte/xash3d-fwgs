"""workflow_sync — mechanical drift checker for the agent-workflow surface.

Invariants are staged:
  stage 1 — must pass at the end of the tooling session (S1).
  stage 2 — additionally requires the adapter-parity / twin-entry-file /
            WORKFLOW.md upgrades session (S2).

Run `--stage 1` during S1; plain run (= stage 2) is the final gate.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

from . import GITHUB, REPO, XPP
from .scan import read_frontmatter

PROMPTS = GITHUB / "prompts"
AGENTS = GITHUB / "agents"
CL_COMMANDS = REPO / ".claude" / "commands"
CL_AGENTS = REPO / ".claude" / "agents"
OC_COMMANDS = REPO / ".opencode" / "command"
TOOLS_DIR = XPP / "tools"

NO_ARG_PROMPTS = {"init", "status-and-next", "dependency-graph"}
PROMPT_FIELD_ORDER = ["name", "description", "argument-hint", "agent", "tools", "model"]
AGENT_FIELD_ORDER = ["name", "description", "tools", "model"]

# Files where "C++20" would be a statement of the project standard (stray
# standard-version *citations* like "std::bit_cast (C++20)" elsewhere are fine).
CPP20_FORBIDDEN = [
    GITHUB / "instructions" / "xash3dpp.instructions.md",
    PROMPTS / "init.prompt.md",
    REPO / "CLAUDE.md",
    REPO / "AGENTS.md",
]


def _finding(stage: int, invariant: str, detail: str) -> dict:
    return {"stage": stage, "invariant": invariant, "detail": detail}


def _norm(s: str) -> str:
    return re.sub(r"\s+", " ", s).strip().lower()


def canonical_models() -> dict[str, dict[str, str]]:
    """Parse the canonical model table from MODEL-GUIDE.md.
    Returns {tier: {copilot, claude, opencode}}."""
    text = (GITHUB / "MODEL-GUIDE.md").read_text(encoding="utf-8", errors="replace")
    out: dict[str, dict[str, str]] = {}
    section = text.split("## Canonical model table", 1)
    if len(section) < 2:
        return out
    in_table = False
    for line in section[1].splitlines():
        if line.startswith("| Tier |"):
            in_table = True
            continue
        if in_table:
            if not line.startswith("|"):
                break
            cells = [c.strip() for c in line.strip("|").split("|")]
            if len(cells) >= 4 and not set(cells[0]) <= {"-", " "}:
                out[cells[0]] = {
                    "copilot": cells[1].strip("`"),
                    "claude": cells[2].strip("`"),
                    "opencode": cells[3].strip("`"),
                }
    return out


def workflow_sync(stage: int = 2) -> dict:
    findings: list[dict] = []
    prompts = sorted(PROMPTS.glob("*.prompt.md"))
    agents = sorted(AGENTS.glob("*.agent.md"))
    models = canonical_models()
    copilot_ids = {m["copilot"] for m in models.values()}
    claude_ids = {m["claude"] for m in models.values()}
    tier_of = {m["copilot"]: t for t, m in models.items()}
    tier_of_claude = {m["claude"]: t for t, m in models.items()}

    if not models:
        findings.append(_finding(1, "model-table",
                                 "canonical model table not found in MODEL-GUIDE.md"))

    # ---- stage 1: prompt frontmatter schema/order -------------------------
    for p in prompts:
        stem = p.name.removesuffix(".prompt.md")
        pairs, _ = read_frontmatter(p)
        keys = [k for k, _ in pairs]
        expected = [f for f in PROMPT_FIELD_ORDER
                    if f != "argument-hint" or "argument-hint" in keys]
        if keys != expected:
            findings.append(_finding(1, "prompt-frontmatter",
                                     "%s: fields %s, expected %s" % (p.name, keys, expected)))
        kv = dict(pairs)
        if kv.get("agent") != "agent":
            findings.append(_finding(1, "prompt-frontmatter",
                                     "%s: 'agent: agent' missing" % p.name))
        if "mode" in kv:
            findings.append(_finding(1, "prompt-frontmatter",
                                     "%s: forbidden 'mode:' field" % p.name))
        if "argument-hint" not in kv and stem not in NO_ARG_PROMPTS:
            findings.append(_finding(1, "prompt-frontmatter",
                                     "%s: argument-hint missing (not in no-arg list)" % p.name))
        if models and kv.get("model") not in copilot_ids:
            findings.append(_finding(1, "model-dialect",
                                     "%s: model %r not in canonical Copilot column"
                                     % (p.name, kv.get("model"))))

    # ---- stage 1: agent frontmatter + model dialects ----------------------
    for a in agents:
        pairs, _ = read_frontmatter(a)
        keys = [k for k, _ in pairs]
        if keys != AGENT_FIELD_ORDER:
            findings.append(_finding(1, "agent-frontmatter",
                                     "%s: fields %s, expected %s" % (a.name, keys, AGENT_FIELD_ORDER)))
        kv = dict(pairs)
        if models and kv.get("model") not in copilot_ids:
            findings.append(_finding(1, "model-dialect",
                                     "%s: model %r not in canonical Copilot column"
                                     % (a.name, kv.get("model"))))
        # tier parity with the Claude adapter
        adapter = CL_AGENTS / (a.name.removesuffix(".agent.md") + ".md")
        if adapter.is_file() and models:
            apairs, _ = read_frontmatter(adapter)
            akv = dict(apairs)
            gtier = tier_of.get(kv.get("model", ""))
            ctier = tier_of_claude.get(akv.get("model", ""))
            if akv.get("model") not in claude_ids:
                findings.append(_finding(1, "model-dialect",
                                         "%s: model %r not in canonical Claude column"
                                         % (adapter.name, akv.get("model"))))
            elif gtier and ctier and gtier != ctier:
                findings.append(_finding(1, "tier-parity",
                                         "%s: .github says %s tier, Claude adapter says %s tier"
                                         % (a.name, gtier, ctier)))

    # ---- stage 1: project-standard statements ----------------------------
    for f in CPP20_FORBIDDEN:
        if f.is_file() and "C++20" in f.read_text(encoding="utf-8", errors="replace"):
            findings.append(_finding(1, "cpp-standard",
                                     "%s still says C++20" % f.name))

    # ---- stage 1: ABI table single-source ---------------------------------
    abi_rows = 0
    for md in GITHUB.rglob("*.md"):
        abi_rows += md.read_text(encoding="utf-8", errors="replace").count("| Game DLL |")
    if abi_rows != 1:
        findings.append(_finding(1, "abi-single-source",
                                 "'| Game DLL |' table row found %d times in .github (want 1)" % abi_rows))
    for a in agents:
        body = a.read_text(encoding="utf-8", errors="replace")
        if a.name != "legacy-parity-auditor.agent.md" and \
                "ABI Surfaces" not in body:
            findings.append(_finding(1, "abi-single-source",
                                     "%s: no reference to the canonical ABI section" % a.name))

    # ---- stage 1: referenced tools exist ----------------------------------
    tool_refs: set[str] = set()
    scan_docs = list(GITHUB.rglob("*.md")) + [REPO / "CLAUDE.md", REPO / "AGENTS.md"]
    for md in scan_docs:
        if md.is_file():
            for m in re.finditer(r"tools/(\w+)\.py", md.read_text(encoding="utf-8", errors="replace")):
                tool_refs.add(m.group(1))
    for name in sorted(tool_refs):
        if not (TOOLS_DIR / ("%s.py" % name)).is_file():
            findings.append(_finding(1, "tools-exist",
                                     "referenced tools/%s.py does not exist" % name))

    # ---- stage 1: Q-count consistency -------------------------------------
    reg = XPP / "docs" / "design" / "decisions-architecture.md"
    reg_text = reg.read_text(encoding="utf-8", errors="replace")
    q_max = max(int(n) for n in re.findall(r"\(Q-(\d+)\)", reg_text))
    words = {"twelve": 12, "thirteen": 13, "fourteen": 14, "fifteen": 15,
             "sixteen": 16, "seventeen": 17, "eighteen": 18, "nineteen": 19,
             "twenty": 20}
    m = re.search(r"All (\w+|\d+) questions", reg_text)
    if m:
        stated = words.get(m.group(1).lower()) or (int(m.group(1)) if m.group(1).isdigit() else None)
        if stated != q_max:
            findings.append(_finding(1, "q-count",
                                     "register says 'All %s questions' but max is Q-%d"
                                     % (m.group(1), q_max)))

    # ---- stage 1: checklist single-source ----------------------------------
    embedded_rx = re.compile(r"-\s*\[\s*\]\s*`xash3dpp/docs/boundaries/")
    for name in ("pre-pr.prompt.md", "finish-subsystem.prompt.md"):
        body = (PROMPTS / name).read_text(encoding="utf-8", errors="replace")
        if "finish_check.py" not in body:
            findings.append(_finding(1, "checklist-single-source",
                                     "%s does not reference finish_check.py" % name))
        if embedded_rx.search(body):
            findings.append(_finding(1, "checklist-single-source",
                                     "%s re-embeds the 9-section checklist body" % name))

    # ---- stage 1: tools README completeness --------------------------------
    readme = TOOLS_DIR / "README.md"
    if readme.is_file():
        rtext = readme.read_text(encoding="utf-8", errors="replace")
        for script in sorted(TOOLS_DIR.glob("*.py")):
            if script.name not in rtext:
                findings.append(_finding(1, "tools-readme",
                                         "tools/README.md does not mention %s" % script.name))
    else:
        findings.append(_finding(1, "tools-readme", "tools/README.md missing"))

    # ---- stage 1: MCP registration parity ----------------------------------
    mcp_files = {
        ".mcp.json": REPO / ".mcp.json",
        ".vscode/mcp.json": REPO / ".vscode" / "mcp.json",
        "opencode.json": REPO / "opencode.json",
    }
    for label, f in mcp_files.items():
        if not f.is_file() or "xash-tools" not in f.read_text(encoding="utf-8", errors="replace"):
            findings.append(_finding(1, "mcp-parity",
                                     "%s missing xash-tools registration" % label))

    # ---- stage 2: adapter parity -------------------------------------------
    for p in prompts:
        stem = p.name.removesuffix(".prompt.md")
        pairs, _ = read_frontmatter(p)
        origin_desc = dict(pairs).get("description", "")
        for kind, adir in (("claude", CL_COMMANDS), ("opencode", OC_COMMANDS)):
            adapter = adir / ("%s.md" % stem)
            if not adapter.is_file():
                findings.append(_finding(2, "adapter-existence",
                                         "no %s command adapter for %s" % (kind, stem)))
                continue
            apairs, abody = read_frontmatter(adapter)
            if ".github/prompts/%s.prompt.md" % stem not in abody:
                findings.append(_finding(2, "adapter-pointer",
                                         "%s (%s): body lacks exact origin path" % (stem, kind)))
            if _norm(dict(apairs).get("description", "")) != _norm(origin_desc):
                findings.append(_finding(2, "description-parity",
                                         "%s (%s): description differs from origin" % (stem, kind)))
    for a in agents:
        stem = a.name.removesuffix(".agent.md")
        adapter = CL_AGENTS / ("%s.md" % stem)
        if not adapter.is_file():
            findings.append(_finding(2, "adapter-existence",
                                     "no Claude agent adapter for %s" % stem))
        else:
            _, abody = read_frontmatter(adapter)
            if ".github/agents/%s.agent.md" % stem not in abody:
                findings.append(_finding(2, "adapter-pointer",
                                         "%s (agent): body lacks exact origin path" % stem))
    for adapter in CL_COMMANDS.glob("*.md"):
        stem = adapter.name.removesuffix(".md")
        if not (PROMPTS / ("%s.prompt.md" % stem)).is_file():
            findings.append(_finding(2, "adapter-existence",
                                     "orphan Claude command adapter %s" % adapter.name))

    # ---- stage 2: twin entry files -----------------------------------------
    claude_md = REPO / "CLAUDE.md"
    agents_md = REPO / "AGENTS.md"
    if not agents_md.is_file():
        findings.append(_finding(2, "twin-files", "root AGENTS.md missing"))
    else:
        blocks: dict[str, dict[str, str]] = {}
        rx = re.compile(r"<!-- SYNC-CORE:BEGIN ([\w-]+) -->(.*?)<!-- SYNC-CORE:END \1 -->",
                        re.DOTALL)
        for label, f in (("CLAUDE.md", claude_md), ("AGENTS.md", agents_md)):
            text = f.read_text(encoding="utf-8", errors="replace")
            blocks[label] = {m.group(1): m.group(2) for m in rx.finditer(text)}
        ids = set(blocks["CLAUDE.md"]) | set(blocks["AGENTS.md"])
        if not ids:
            findings.append(_finding(2, "twin-files", "no SYNC-CORE blocks found"))
        for bid in sorted(ids):
            a, b = blocks["CLAUDE.md"].get(bid), blocks["AGENTS.md"].get(bid)
            if a is None or b is None:
                findings.append(_finding(2, "twin-files",
                                         "SYNC-CORE block %r present in only one file" % bid))
            elif a != b:
                findings.append(_finding(2, "twin-files",
                                         "SYNC-CORE block %r differs between files" % bid))

    # ---- stage 2: CLAUDE.md lists match .claude/ ----------------------------
    if claude_md.is_file():
        ctext = claude_md.read_text(encoding="utf-8", errors="replace")
        for cmd in sorted(CL_COMMANDS.glob("*.md")):
            if "/%s" % cmd.name.removesuffix(".md") not in ctext:
                findings.append(_finding(2, "claude-lists",
                                         "CLAUDE.md does not list /%s" % cmd.name.removesuffix(".md")))

    # ---- stage 2: WORKFLOW table completeness + Q cite ----------------------
    wf = GITHUB / "WORKFLOW.md"
    wtext = wf.read_text(encoding="utf-8", errors="replace")
    for p in prompts:
        stem = p.name.removesuffix(".prompt.md")
        if "`%s`" % stem not in wtext and stem not in wtext:
            findings.append(_finding(2, "workflow-table",
                                     "WORKFLOW.md prompt table missing %s" % stem))
    for a in agents:
        stem = a.name.removesuffix(".agent.md")
        if stem not in wtext:
            findings.append(_finding(2, "workflow-table",
                                     "WORKFLOW.md missing agent %s" % stem))
    m = re.search(r"Q-1 through Q-(\d+)", wtext)
    if m and int(m.group(1)) != q_max:
        findings.append(_finding(2, "q-count",
                                 "WORKFLOW.md cites Q-1 through Q-%s but max is Q-%d"
                                 % (m.group(1), q_max)))

    selected = [f for f in findings if f["stage"] <= stage]
    return {
        "stage": stage,
        "findings": selected,
        "counts": {
            "stage1": sum(1 for f in findings if f["stage"] == 1),
            "stage2": sum(1 for f in findings if f["stage"] == 2),
        },
    }
