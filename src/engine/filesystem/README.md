# Engine Filesystem Bridge

Reserved for engine-side filesystem bridge helpers around
`engine/common/filesystem_engine.c`.

This is distinct from `src/filesystem/`, which owns modern internals for the
standalone `filesystem_stdio` module.

Current helpers:

- `mount_flags.cpp`: target-neutral construction of mount flags from engine
  cvar selections.
- `mount_flags_adapter.cpp`: C bridge used by `filesystem_engine.c`, with ABI
  bit checks kept at the legacy boundary.
