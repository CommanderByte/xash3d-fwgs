# Engine Memory TODO

## Purpose

Plan a cautious modernization path for engine memory pools and allocation
helpers. This is intentionally later in the backlog because allocator mistakes
have high blast radius and often fail far from the cause.

## Scope

Candidate areas:

- `engine/common/zone.c`
- `Mem_AllocPool`, `Mem_FreePool`
- `Mem_Malloc`, `Mem_Calloc`, `Mem_Realloc`, `Mem_Free`
- `Z_Malloc`, `Z_Free`
- `copystring`, `copystringpool`, and owned-string helpers
- allocation stats and debug reporting

## Method

- Audit before implementation.
- Add tests around lifecycle and failure-like edge cases.
- Extract tiny policy-free helpers first.
- Avoid replacing the allocator wholesale until test coverage and diagnostics
  are strong.

## Phase 42 Tasks: Memory Pools And Allocation

- [ ] `ENG-MEM-001` Audit allocation families, ownership rules, pool lifecycle,
  debug reporting, shutdown behavior, and callers that depend on current
  allocation quirks.
  Evidence:

- [ ] `ENG-MEM-002` Add tests for pool allocation/free lifecycle, realloc,
  zero-size or null-pointer behavior, string duplication ownership, and pool
  shutdown cleanup.
  Evidence:

- [ ] `ENG-MEM-003` Add memory snapshot/debug records that can be compared in
  tests without relying on console text.
  Evidence:

- [ ] `ENG-MEM-004` Extract or rewrite a tiny helper group first, such as
  owned string duplication or stats formatting.
  Evidence:

- [ ] `ENG-MEM-005` Decide whether a modern `MemoryPool` abstraction is worth
  introducing, and document exception/RTTI/thread-safety policy before any
  broad allocator migration.
  Evidence:

- [ ] `ENG-MEM-006` Run focused tests, full tests, and extended runtime smoke
  after any allocator-path change.
  Evidence:
