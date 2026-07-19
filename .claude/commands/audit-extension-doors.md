---
description: Read-only audit of one subsystem's extension posture: verifies the boundary spec's 'Extension axes (Q-21)' section covers the current north-star axis set (G-*/P-* in extension-goals.md), that its claims match the code, and that door-rule deviations are recorded as door-debt. Produces a per-axis verdict table and stops. No files are changed.
---

Read `.github/prompts/audit-extension-doors.prompt.md` and execute it exactly as
written — it is the authoritative definition of this workflow step (this command
is a thin Claude Code adapter; do not improvise beyond it). Arguments: $ARGUMENTS
