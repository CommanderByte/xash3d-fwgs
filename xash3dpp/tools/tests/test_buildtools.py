"""Regression tests for the `test` tool's failure-diagnostics helpers.

Stdlib `unittest` only (the xash3dpp/tools CLI path is stdlib-only by contract,
see tools/README.md) and filesystem-free: every target is a pure function over
a line list.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
or from xash3dpp/tools/:
    python -m unittest tests.test_buildtools
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Make the sibling `xtools` package importable (tools/ is tests/'s parent).
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.buildtools import (  # noqa: E402
    _assert_tail, _decode_exit, _failed_output_blocks, _resolve)


class Resolve(unittest.TestCase):
    """(configuration, architecture) -> (configure, build/test, binaryDir)."""

    def test_default_is_debug_x64(self):
        self.assertEqual(_resolve("debug", "x64"),
                         ("debug-msvc", "debug", "Debug"))

    def test_debug_x86(self):
        self.assertEqual(_resolve("debug", "x86"),
                         ("debug-msvc-x86", "debug-x86", "Debug-x86"))

    def test_release_x64(self):
        # fixes the old latent bug: release now configures release-msvc into
        # build/Release, not debug-msvc into build/Debug.
        self.assertEqual(_resolve("release", "x64"),
                         ("release-msvc", "release", "Release"))

    def test_arch_aliases_normalize(self):
        self.assertEqual(_resolve("debug", "win32"), _resolve("debug", "x86"))
        self.assertEqual(_resolve("debug", "amd64"), _resolve("debug", "x64"))

    def test_suffixed_preset_infers_arch(self):
        # the older `--preset debug-x86` form keeps working with arch left x64.
        self.assertEqual(_resolve("debug-x86", "x64"),
                         _resolve("debug", "x86"))

    def test_case_and_whitespace_tolerant(self):
        self.assertEqual(_resolve("  Debug ", " X86 "),
                         _resolve("debug", "x86"))

    def test_unknown_pair_falls_back(self):
        # unknown arch for a known configuration -> that configuration's x64.
        self.assertEqual(_resolve("release", "sparc"),
                         _resolve("release", "x64"))
        # entirely unknown -> debug x64.
        self.assertEqual(_resolve("nonsense", "nonsense"),
                         _resolve("debug", "x64"))

    def test_empty_inputs_default(self):
        self.assertEqual(_resolve("", ""), _resolve("debug", "x64"))


class AssertTail(unittest.TestCase):
    def test_require_line_is_windowed(self):
        out = [
            "  [ run ] test_thing",
            "some setup chatter",
            "more chatter",
            "FATAL [client_state.cpp:456]: cl.frames != nullptr",
        ]
        tail = _assert_tail(out)
        self.assertIn(
            "FATAL [client_state.cpp:456]: cl.frames != nullptr", tail)
        self.assertIn("more chatter", tail)  # a couple lead-in lines for context
        self.assertLessEqual(len(tail), 8)

    def test_check_failure_matched(self):
        out = ["ok stuff", "FAIL [snapshot.cpp:12]: a == b"]
        self.assertIn("FAIL [snapshot.cpp:12]: a == b", _assert_tail(out))

    def test_crt_assert_abort_matched(self):
        out = [
            "running...",
            "Assertion failed: dt.initialized, file delta.cpp, line 88",
            "",
        ]
        tail = _assert_tail(out)
        self.assertTrue(any("Assertion failed" in ln for ln in tail))

    def test_no_signature_falls_back_to_last_lines(self):
        out = ["l%d" % i for i in range(20)]
        self.assertEqual(_assert_tail(out, limit=8),
                         ["l%d" % i for i in range(12, 20)])

    def test_blank_output_is_empty(self):
        self.assertEqual(_assert_tail(["", "   ", "\t"]), [])
        self.assertEqual(_assert_tail([]), [])

    def test_limit_is_respected(self):
        out = ["noise", "noise", "noise", "abort() was called"] + \
            ["trailing"] * 20
        self.assertLessEqual(len(_assert_tail(out, limit=8)), 8)


class DecodeExit(unittest.TestCase):
    def test_abort_code_3(self):
        self.assertIsNotNone(_decode_exit("child exited with code 3"))
        self.assertIn("abort", _decode_exit("code 3").lower())

    def test_access_violation(self):
        d = _decode_exit("Exception 0xC0000005 thrown")
        self.assertIsNotNone(d)
        self.assertIn("ACCESS_VIOLATION", d)

    def test_unrecognized_is_none(self):
        self.assertIsNone(_decode_exit("all good, 0 failed"))


class FailedOutputBlocks(unittest.TestCase):
    def test_block_captured_between_markers(self):
        lines = [
            "1/2 Test #1: test_a .....   Passed  0.01 sec",
            "2/2 Test #2: test_b .....***Failed  0.02 sec",
            "FAIL [x.cpp:1]: boom",
            "extra detail",
            "100% tests passed, 1 tests failed out of 2",
        ]
        blocks = _failed_output_blocks(lines)
        self.assertIn("test_b", blocks)
        self.assertIn("FAIL [x.cpp:1]: boom", blocks["test_b"])
        # the summary line terminates the block
        self.assertNotIn(
            "100% tests passed, 1 tests failed out of 2", blocks["test_b"])


if __name__ == "__main__":
    unittest.main()
