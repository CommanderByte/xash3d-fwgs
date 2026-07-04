"""Locate Visual Studio tooling (cmake/ctest/VsDevCmd/clangd) on Windows.

Resolution order for every path: explicit env override, then vswhere, then
PATH. Env overrides (documented in tools/README.md and AGENT-SETUP.md):

    XASH_CMAKE     full path to cmake.exe
    XASH_CTEST     full path to ctest.exe
    XASH_VSDEVCMD  full path to VsDevCmd.bat
    XASH_CLANGD    full path to clangd.exe
"""

from __future__ import annotations

import os
import shutil
from functools import lru_cache
from pathlib import Path

from .proc import run

VSWHERE = Path(
    os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"

_CMAKE_REL = Path("Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin")
_VSDEVCMD_REL = Path("Common7/Tools/VsDevCmd.bat")
_CLANGD_REL = Path("VC/Tools/Llvm/x64/bin/clangd.exe")


@lru_cache(maxsize=1)
def vs_install_path() -> Path | None:
    if not VSWHERE.is_file():
        return None
    rc, lines, _ = run(
        [str(VSWHERE), "-latest", "-products", "*", "-property", "installationPath"]
    )
    for line in lines:
        line = line.strip()
        if rc == 0 and line and Path(line).is_dir():
            return Path(line)
    return None


def _resolve(env_var: str, vs_relative: Path, which_name: str | None) -> Path:
    override = os.environ.get(env_var)
    if override:
        p = Path(override)
        if p.is_file():
            return p
        raise FileNotFoundError("%s=%s does not exist" % (env_var, override))
    vs = vs_install_path()
    if vs is not None:
        candidate = vs / vs_relative
        if candidate.is_file():
            return candidate
    if which_name:
        found = shutil.which(which_name)
        if found:
            return Path(found)
    raise FileNotFoundError(
        "could not locate %s (set %s, install VS2022, or add to PATH)"
        % (vs_relative.name, env_var)
    )


def cmake_path() -> Path:
    return _resolve("XASH_CMAKE", _CMAKE_REL / "cmake.exe", "cmake")


def ctest_path() -> Path:
    return _resolve("XASH_CTEST", _CMAKE_REL / "ctest.exe", "ctest")


def vsdevcmd_path() -> Path:
    return _resolve("XASH_VSDEVCMD", _VSDEVCMD_REL, None)


def clangd_path() -> Path:
    return _resolve("XASH_CLANGD", _CLANGD_REL, "clangd")
