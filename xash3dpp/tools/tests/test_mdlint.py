"""Regression tests for the markdown_lint wrapper's pure helpers.

Stdlib `unittest` only and filesystem-free (contract: tools/README.md) —
the subprocess itself is not exercised here; only the invocation-building
logic is.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
or from xash3dpp/tools/:
    python -m unittest tests.test_mdlint
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Make the sibling `xtools` package importable (tools/ is tests/'s parent).
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.mdlint import CONFIG, _build_cmd  # noqa: E402


class BuildCmd(unittest.TestCase):
    """`_build_cmd`: the exact pymarkdown invocation shape."""

    def test_shape(self):
        cmd = _build_cmd("py.exe", "cfg.json", ["a.md", "b.md"])
        self.assertEqual(cmd, ["py.exe", "-m", "pymarkdown",
                               "--config", "cfg.json", "scan", "a.md", "b.md"])

    def test_config_flag_precedes_scan(self):
        # --config must come BEFORE the scan subcommand: pymarkdown's
        # discovery is cwd-relative and silently ignores the repo config
        # otherwise (the 2026-07-06 B2 bug this module's docstring records).
        cmd = _build_cmd("py", "cfg", ["x.md"])
        self.assertLess(cmd.index("--config"), cmd.index("scan"))

    def test_config_points_at_xash3dpp_pymarkdown_json(self):
        self.assertEqual(CONFIG.name, ".pymarkdown.json")
        self.assertEqual(CONFIG.parent.name, "xash3dpp")


if __name__ == "__main__":
    unittest.main()
