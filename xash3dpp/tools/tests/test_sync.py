"""Regression tests for workflow_sync helper invariants."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.sync import (  # noqa: E402
    _has_actor,
    _tools_allow_edit,
    _vscode_mcp_discovery_disabled,
    _workflow_edit_cell_ok,
)


class WorkflowSyncHelpers(unittest.TestCase):
    def test_vscode_discovery_accepts_boolean_false(self):
        self.assertTrue(
            _vscode_mcp_discovery_disabled('"chat.mcp.discovery.enabled": false'))

    def test_vscode_discovery_accepts_all_false_object(self):
        text = '"chat.mcp.discovery.enabled": {"claude-desktop": false, "windsurf": false}'
        self.assertTrue(_vscode_mcp_discovery_disabled(text))

    def test_vscode_discovery_rejects_true_object_member(self):
        text = '"chat.mcp.discovery.enabled": {"claude-desktop": false, "windsurf": true}'
        self.assertFalse(_vscode_mcp_discovery_disabled(text))

    def test_actor_requires_env_key_and_actor_value(self):
        self.assertTrue(_has_actor('env = { "XASH_CHECKPOINT_ACTOR" = "codex" }',
                                   "codex"))
        self.assertFalse(_has_actor('env = { "OTHER" = "codex" }', "codex"))

    def test_edit_expectation_follows_prompt_tools(self):
        self.assertTrue(_tools_allow_edit("[read, search, edit]"))
        self.assertFalse(_tools_allow_edit("[read, search, execute]"))
        self.assertTrue(_workflow_edit_cell_ok("[read, edit]", "Yes (docs only)"))
        self.assertTrue(_workflow_edit_cell_ok("[read, search]", "No"))
        self.assertFalse(_workflow_edit_cell_ok("[read, edit]", "No"))


if __name__ == "__main__":
    unittest.main()
