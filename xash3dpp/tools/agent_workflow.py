#!/usr/bin/env python
"""Framework-aware helper for the xash3dpp agent workflow.

The .github/prompts/*.prompt.md files are canonical. This tool only lists
them, shows their metadata/body, and prints the correct invocation shape for a
specific agent framework.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from xtools import GITHUB, REPO
from xtools.scan import read_frontmatter


PROMPTS = GITHUB / "prompts"
FRAMEWORKS = ("claude", "copilot", "opencode", "codex")
COAUTHOR_DEFAULTS = {
    "claude": ("Claude Fable 5", "noreply@anthropic.com"),
    "copilot": ("GitHub Copilot", "noreply@github.com"),
    "opencode": ("opencode agent", "noreply@opencode.ai"),
    "codex": ("Codex GPT-5", "noreply@openai.com"),
}


def _prompt_files() -> list[Path]:
    return sorted(PROMPTS.glob("*.prompt.md"))


def _stem(path: Path) -> str:
    return path.name.removesuffix(".prompt.md")


def _prompt_path(name: str) -> Path:
    key = name.removeprefix("/").removesuffix(".prompt.md").removesuffix(".md")
    path = PROMPTS / ("%s.prompt.md" % key)
    if path.is_file():
        return path
    known = ", ".join(_stem(p) for p in _prompt_files())
    raise SystemExit("unknown prompt %r (known: %s)" % (name, known))


def _metadata(path: Path) -> dict[str, str]:
    pairs, _ = read_frontmatter(path)
    data = dict(pairs)
    data["stem"] = _stem(path)
    data["path"] = str(path.relative_to(REPO)).replace("\\", "/")
    return data


def _ps_quote(text: str) -> str:
    return "'" + text.replace("'", "''") + "'"


def _command(framework: str, stem: str, args: str) -> dict[str, str]:
    origin = ".github/prompts/%s.prompt.md" % stem
    suffix = (" " + args) if args else ""
    if framework == "claude":
        return {
            "framework": framework,
            "command": "/%s%s" % (stem, suffix),
            "notes": "Claude Code uses the thin adapter at .claude/commands/%s.md." % stem,
        }
    if framework == "copilot":
        return {
            "framework": framework,
            "command": "/%s%s" % (stem, suffix),
            "notes": "VS Code Copilot invokes the canonical prompt file in .github/prompts/.",
        }
    if framework == "opencode":
        return {
            "framework": framework,
            "command": "/%s%s" % (stem, suffix),
            "notes": "opencode uses the thin adapter at .opencode/commands/%s.md." % stem,
        }
    if framework == "codex":
        prompt = "Read %s and execute it exactly as written. Arguments: %s" % (
            origin,
            args if args else "(none)",
        )
        return {
            "framework": framework,
            "command": "codex exec -C %s %s" % (REPO, _ps_quote(prompt)),
            "notes": (
                "Codex has no repo slash-command adapter; the command reads "
                "the canonical prompt directly."
            ),
        }
    raise SystemExit("unknown framework %r (known: %s)" %
                     (framework, ", ".join(FRAMEWORKS)))


def _coauthor(framework: str, model: str = "", name: str = "",
              email: str = "") -> str:
    default_name, default_email = COAUTHOR_DEFAULTS[framework]
    model = model or os.environ.get("XASH_AGENT_MODEL", "")
    if not name:
        if model:
            base = {
                "claude": "Claude",
                "copilot": "GitHub Copilot",
                "opencode": "opencode",
                "codex": "Codex",
            }[framework]
            name = "%s %s" % (base, model)
        else:
            name = default_name
    email = email or default_email
    return "Co-Authored-By: %s <%s>" % (name, email)


def cmd_list(args: argparse.Namespace) -> int:
    rows = [_metadata(p) for p in _prompt_files()]
    if args.json:
        print(json.dumps(rows, indent=2))
        return 0
    for row in rows:
        print("%-32s %s" % (row["stem"], row.get("description", "")))
    return 0


def cmd_show(args: argparse.Namespace) -> int:
    path = _prompt_path(args.prompt)
    meta = _metadata(path)
    pairs, body = read_frontmatter(path)
    data = {"metadata": meta, "frontmatter": pairs}
    if args.body:
        data["body"] = body
    if args.json:
        print(json.dumps(data, indent=2))
        return 0
    print("Prompt: %s" % meta["stem"])
    print("Path: %s" % meta["path"])
    for key in ("name", "description", "argument-hint", "tools", "model"):
        if key in meta:
            print("%s: %s" % (key, meta[key]))
    if args.body:
        print()
        print(body)
    return 0


def cmd_command(args: argparse.Namespace) -> int:
    path = _prompt_path(args.prompt)
    meta = _metadata(path)
    joined_args = " ".join(args.arguments)
    data = _command(args.framework, meta["stem"], joined_args)
    data["prompt"] = meta["stem"]
    data["origin"] = meta["path"]
    if args.json:
        print(json.dumps(data, indent=2))
        return 0
    print(data["command"])
    print(data["notes"])
    return 0


def cmd_coauthor(args: argparse.Namespace) -> int:
    trailer = _coauthor(args.framework, args.model, args.name, args.email)
    data = {"framework": args.framework, "trailer": trailer}
    if args.json:
        print(json.dumps(data, indent=2))
        return 0
    print(trailer)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)

    list_ap = sub.add_parser("list", help="list canonical workflow prompts")
    list_ap.add_argument("--json", action="store_true")
    list_ap.set_defaults(func=cmd_list)

    show_ap = sub.add_parser("show", help="show prompt metadata")
    show_ap.add_argument("prompt")
    show_ap.add_argument("--body", action="store_true")
    show_ap.add_argument("--json", action="store_true")
    show_ap.set_defaults(func=cmd_show)

    command_ap = sub.add_parser("command", help="print framework invocation")
    command_ap.add_argument("framework", choices=FRAMEWORKS)
    command_ap.add_argument("prompt")
    command_ap.add_argument("arguments", nargs="*")
    command_ap.add_argument("--json", action="store_true")
    command_ap.set_defaults(func=cmd_command)

    coauthor_ap = sub.add_parser("coauthor", help="print commit co-author trailer")
    coauthor_ap.add_argument("framework", choices=FRAMEWORKS)
    coauthor_ap.add_argument("model", nargs="?", default="",
                             help="model display name, e.g. GPT-5 or Claude Sonnet 4.6")
    coauthor_ap.add_argument("--name", default="",
                             help="override full co-author display name")
    coauthor_ap.add_argument("--email", default="",
                             help="override co-author e-mail")
    coauthor_ap.add_argument("--json", action="store_true")
    coauthor_ap.set_defaults(func=cmd_coauthor)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
