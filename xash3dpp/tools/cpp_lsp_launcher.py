#!/usr/bin/env python
"""Portable launcher for the cpp-lsp MCP server (mcp-language-server +
clangd over xash3dpp/).

Replaces the machine-specific absolute paths that used to live in
.mcp.json: mcp-language-server is resolved via XASH_MCP_LANGUAGE_SERVER,
PATH, then ~/go/bin; clangd via XASH_CLANGD, vswhere (VS2022 bundled LLVM),
then PATH. The process is exec-replaced so stdio passes straight through.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from xtools import XPP, env_path  # noqa: E402
from xtools.vsenv import clangd_path  # noqa: E402


def find_language_server() -> Path:
    override = env_path("XASH_MCP_LANGUAGE_SERVER")
    if override and override.is_file():
        return override
    found = shutil.which("mcp-language-server")
    if found:
        return Path(found)
    go_bin = Path.home() / "go" / "bin" / "mcp-language-server.exe"
    if go_bin.is_file():
        return go_bin
    raise FileNotFoundError(
        "mcp-language-server not found: set XASH_MCP_LANGUAGE_SERVER, add it "
        "to PATH, or `go install github.com/isaacphi/mcp-language-server@latest`"
    )


def main() -> int:
    server = find_language_server()
    clangd = clangd_path()
    compile_db = XPP / "build" / "clangd"
    cmd = [
        str(server),
        "--workspace", str(XPP),
        "--lsp", str(clangd),
        "--",
        "--compile-commands-dir=%s" % compile_db,
        "--background-index",
        "--header-insertion=never",
    ]
    # exec-style replacement keeps stdio wiring transparent to the MCP host
    return subprocess.call(cmd, stdin=sys.stdin, stdout=sys.stdout,
                           stderr=sys.stderr)


if __name__ == "__main__":
    sys.exit(main())
