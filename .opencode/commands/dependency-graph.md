---
description: Read all InitParams structs and EngineContext member declarations to build the subsystem dependency graph, check for cycles, and verify that member init order in EngineContext is consistent with the dependency direction. Read-only.
---

Read `.github/prompts/dependency-graph.prompt.md` and execute it exactly as written — it
is the authoritative definition of this workflow step (this command is a thin
opencode adapter; do not improvise beyond it). Arguments: $ARGUMENTS
