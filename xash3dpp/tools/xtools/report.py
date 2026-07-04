"""Envelope, human-readable printing, and exit-code policy for all CLIs.

Every tool emits:  {"tool", "version", "ok", "data", "errors"}
Exit codes:        0 = clean, 1 = findings/failures present, 2 = execution error.
"""

from __future__ import annotations

import json
import sys
import traceback
from typing import Any, Callable

from . import TOOL_VERSION


def envelope(tool: str, ok: bool, data: dict, errors: list[str] | None = None) -> dict:
    return {
        "tool": tool,
        "version": TOOL_VERSION,
        "ok": ok,
        "data": data,
        "errors": errors or [],
    }


def print_table(rows: list[dict], columns: list[str]) -> None:
    if not rows:
        return
    widths = {c: max(len(c), *(len(str(r.get(c, ""))) for r in rows)) for c in columns}
    line = "  ".join(c.ljust(widths[c]) for c in columns)
    print(line)
    print("  ".join("-" * widths[c] for c in columns))
    for r in rows:
        print("  ".join(str(r.get(c, "")).ljust(widths[c]) for c in columns))


def cli_main(
    tool: str,
    run: Callable[[], tuple[bool, dict]],
    as_json: bool,
    human: Callable[[dict], None] | None = None,
) -> int:
    """Run a tool function, print envelope (or human view), return exit code.

    `run` returns (clean, data): clean=True -> exit 0, else exit 1.
    Unexpected exceptions -> exit 2 with the error in the envelope.
    """
    try:
        clean, data = run()
        env = envelope(tool, clean, data)
        if as_json or human is None:
            print(json.dumps(env, indent=2))
        else:
            human(data)
            print("\n[%s] %s" % (tool, "clean" if clean else "findings present"))
        return 0 if clean else 1
    except Exception as exc:  # noqa: BLE001 - tool boundary
        env = envelope(tool, False, {}, ["%s: %s" % (type(exc).__name__, exc)])
        print(json.dumps(env, indent=2))
        traceback.print_exc(file=sys.stderr)
        return 2
