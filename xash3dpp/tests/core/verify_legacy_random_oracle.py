#!/usr/bin/env python3
"""Fail unless the C oracle algorithm is the legacy source block verbatim."""

from __future__ import annotations

import pathlib
import sys


def normalized(path: pathlib.Path) -> bytes:
    return path.read_bytes().replace(b"\r\n", b"\n")


def between(data: bytes, begin: bytes, end: bytes) -> bytes:
    start = data.index(begin) + len(begin)
    stop = data.index(end, start)
    return data[start:stop].strip(b"\n")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_legacy_random_oracle.py <common.c> <oracle.c>")
        return 2

    source = normalized(pathlib.Path(sys.argv[1]))
    oracle = normalized(pathlib.Path(sys.argv[2]))

    source_start = source.index(b"static int idum = 0;")
    source_stop = source.index(
        b"\n\n/*\n============\nva", source_start
    )
    source_block = source[source_start:source_stop]
    oracle_block = between(
        oracle,
        b"/* XASH3DPP_LEGACY_RANDOM_ORACLE_BEGIN */\n",
        b"\n/* XASH3DPP_LEGACY_RANDOM_ORACLE_END */",
    )

    if source_block != oracle_block:
        print("legacy RNG oracle differs from engine/common/common.c:54-153")
        return 1
    print("legacy RNG oracle: byte-identical (line endings normalized)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
