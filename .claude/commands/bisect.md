---
description: Run git bisect to locate the exact commit that introduced a build failure, test regression, or behavioural change. Provide a failing symptom and an optional known-good commit. Read-only analysis; does not modify source files.
---

Read `.github/prompts/bisect.prompt.md` and execute it exactly as written — it
is the authoritative definition of this workflow step (this command is a thin
Claude Code adapter; do not improvise beyond it). Arguments: $ARGUMENTS
