# Modern Debugging Utilities

This folder contains detailed architecture notes for the shared debugging
utility layer intended for the rewrite.

Documents:

- [architecture.md](architecture.md) describes the planned module boundaries,
  object roles, threading model, serialization flow, and first filesystem
  integration path.
- [api-inventory.md](api-inventory.md) lists the proposed namespaces, classes,
  structs, and first implementation order.
- [trace-gating-policy.md](trace-gating-policy.md) defines the compile-time
  and runtime trace gating policy before trace macros or async producers are
  added.
- [formatting-policy.md](formatting-policy.md) records where `{fmt}` or a
  similar formatting backend should fit later.
- [thread-safety-audit.md](thread-safety-audit.md) records the current
  multithreading guarantees and remaining caveats.

Related source homes:

- `src/debugging/`
- `src/include/debugging/`
