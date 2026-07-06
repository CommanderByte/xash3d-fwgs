"""Regression tests for the framework-aware workflow helper."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from agent_workflow import _coauthor, _command, _prompt_path  # noqa: E402


class AgentWorkflowTests(unittest.TestCase):
    def test_prompt_path_accepts_slash_command_spelling(self):
        path = _prompt_path("/init")
        self.assertEqual(path.name, "init.prompt.md")

    def test_codex_command_reads_canonical_prompt(self):
        data = _command("codex", "init", "")
        self.assertIn("codex exec -C", data["command"])
        self.assertIn(".github/prompts/init.prompt.md", data["command"])
        self.assertIn("Arguments: (none)", data["command"])

    def test_adapter_frameworks_use_slash_command(self):
        for framework in ("claude", "copilot", "opencode"):
            with self.subTest(framework=framework):
                data = _command(framework, "status-and-next", "")
                self.assertEqual(data["command"], "/status-and-next")

    def test_arguments_are_passed_through(self):
        data = _command("claude", "finish-subsystem", "server")
        self.assertEqual(data["command"], "/finish-subsystem server")

    def test_coauthor_defaults_name_agent_and_model(self):
        self.assertEqual(
            _coauthor("codex"),
            "Co-Authored-By: Codex GPT-5 <noreply@openai.com>")

    def test_coauthor_model_override_names_framework_and_model(self):
        self.assertEqual(
            _coauthor("copilot", model="Claude Sonnet 4.6"),
            "Co-Authored-By: GitHub Copilot Claude Sonnet 4.6 <noreply@github.com>")

    def test_coauthor_full_override(self):
        self.assertEqual(
            _coauthor("opencode", name="opencode Claude Sonnet 4.6",
                      email="agent@example.invalid"),
            "Co-Authored-By: opencode Claude Sonnet 4.6 <agent@example.invalid>")


if __name__ == "__main__":
    unittest.main()
