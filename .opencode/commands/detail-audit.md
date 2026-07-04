---
description: Read-only structural audit of an xash3dpp module. Runs six checks (limits.hpp coverage, header placement, memory/pool integration, stats tiering, dependency injection, compat isolation), produces a numbered violations table, and stops. No source files are changed. Use /implement-audit to apply the identified fixes.
---

Read `.github/prompts/detail-audit.prompt.md` and execute it exactly as written — it
is the authoritative definition of this workflow step (this command is a thin
opencode adapter; do not improvise beyond it). Arguments: $ARGUMENTS
