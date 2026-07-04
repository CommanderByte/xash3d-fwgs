---
description: Produce or update technical architecture documentation for a xash3dpp submodule. Creates docs/architecture/<module>/ with README.md (overview), index.md (concept/file index), and one detailed .md per major concept or source file. If docs already exist, audits them for staleness and updates rather than overwrites. Use when a subsystem is complete enough to document, when onboarding needs a reference, or when the subsystem has changed and docs need refreshing.
---

Read `.github/prompts/document-architecture.prompt.md` and execute it exactly as written — it
is the authoritative definition of this workflow step (this command is a thin
opencode adapter; do not improvise beyond it). Arguments: $ARGUMENTS
