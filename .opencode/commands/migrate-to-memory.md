---
description: Audit a xash3dpp subsystem for direct malloc/free/new/delete usage and migrate all allocation sites to the xash3dpp memory subsystem (create_pool, mem_alloc, mem_free, pool_new, pool_delete). Use when writing a new subsystem or porting an existing one.
---

Read `.github/prompts/migrate-to-memory.prompt.md` and execute it exactly as written — it
is the authoritative definition of this workflow step (this command is a thin
opencode adapter; do not improvise beyond it). Arguments: $ARGUMENTS
