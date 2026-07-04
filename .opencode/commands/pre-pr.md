---
description: One-shot PR readiness check for a completed xash3dpp subsystem. Runs finish-subsystem checklist, a targeted compliance scan, and the reviewer agent in sequence. Reports a single SHIP / HOLD verdict. Run immediately before opening a pull request.
---

Read `.github/prompts/pre-pr.prompt.md` and execute it exactly as written — it
is the authoritative definition of this workflow step (this command is a thin
opencode adapter; do not improvise beyond it). Arguments: $ARGUMENTS
