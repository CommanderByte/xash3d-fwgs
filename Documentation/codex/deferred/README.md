# Deferred Modernization Work

This folder tracks useful modernization work that is intentionally paused
because another subsystem owns the important design decisions.

Deferred work is not abandoned. Keep enough context here to restart it later,
but avoid treating these documents as active implementation checklists.

## Documents

- [todo/](todo/README.md) contains deferred TODO lists.

## Rules

- Record the blocking owner or decision.
- Prefer a concrete resume condition over a vague "later".
- Do not route code through new facades just to satisfy a deferred TODO.
