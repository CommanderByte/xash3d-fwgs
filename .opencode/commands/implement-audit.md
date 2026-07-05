---
description: Applies all structural violations found by /detail-audit to a module. Re-derives the violations internally (same eight checks), applies every finding in severity order (BLOCKERs first, WARNINGs second), updates the boundary doc if one exists, builds, runs tests, and commits. Invoke this after reviewing /detail-audit output, or directly to run the full audit-fix-test-commit cycle without a review gate.
---

Read `.github/prompts/implement-audit.prompt.md` and execute it exactly as written — it
is the authoritative definition of this workflow step (this command is a thin
opencode adapter; do not improvise beyond it). Arguments: $ARGUMENTS
