"""File walkers and a comment/string-aware line iterator for C++ sources."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterator

from . import INCLUDE, PRIVATE, SRC, TESTS

CPP_EXT = {".cpp", ".hpp", ".h", ".c", ".inl"}


def subsystem_files(
    sub: str,
    src: bool = True,
    include: bool = True,
    tests: bool = False,
) -> list[Path]:
    """All C++ files belonging to one subsystem, sorted."""
    roots: list[Path] = []
    if src:
        roots.append(SRC / sub)
    if include:
        roots.append(INCLUDE / sub)
        roots.append(PRIVATE / sub)
    if tests:
        roots.append(TESTS / sub)
    out: list[Path] = []
    for root in roots:
        if root.is_dir():
            out.extend(p for p in root.rglob("*") if p.suffix in CPP_EXT)
    return sorted(out)


_LINE_COMMENT = re.compile(r"//.*$")
_STRING = re.compile(r'"(?:[^"\\]|\\.)*"')
_CHAR = re.compile(r"'(?:[^'\\]|\\.)'")


def code_lines(path: Path) -> Iterator[tuple[int, str, str]]:
    """Yield (lineno, code_only, raw) with comments and string/char literal
    contents blanked out of `code_only`. Block comments are tracked across
    lines. Not a full lexer (raw strings are not special-cased) but good
    enough for convention scanning without comment false-positives.
    """
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return
    in_block = False
    for lineno, raw in enumerate(text.splitlines(), start=1):
        line = raw
        if in_block:
            end = line.find("*/")
            if end == -1:
                yield lineno, "", raw
                continue
            line = " " * (end + 2) + line[end + 2 :]
            in_block = False
        # strip any /* ... */ pairs on this line; detect unterminated opener
        while True:
            start = line.find("/*")
            if start == -1:
                break
            end = line.find("*/", start + 2)
            if end == -1:
                line = line[:start]
                in_block = True
                break
            line = line[:start] + " " * (end + 2 - start) + line[end + 2 :]
        line = _STRING.sub('""', line)
        line = _CHAR.sub("''", line)
        line = _LINE_COMMENT.sub("", line)
        yield lineno, line, raw


def read_frontmatter(path: Path) -> tuple[list[tuple[str, str]], str]:
    """Parse a markdown file's YAML-ish frontmatter into ordered (key, value)
    pairs (top-level keys only) plus the body. Tolerant, not a YAML parser —
    the workflow files only use scalar/inline-list values.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    if not text.startswith("---"):
        return [], text
    lines = text.splitlines()
    pairs: list[tuple[str, str]] = []
    body_start = len(lines)
    for i, line in enumerate(lines[1:], start=1):
        if line.strip() == "---":
            body_start = i + 1
            break
        m = re.match(r"^([A-Za-z][\w-]*):\s*(.*)$", line)
        if m:
            pairs.append((m.group(1), m.group(2).strip().strip('"')))
    return pairs, "\n".join(lines[body_start:])
