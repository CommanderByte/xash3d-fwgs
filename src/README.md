# Source Modernization Area

This folder is reserved for new reusable C++ internals introduced by the
modularization work.

It is now build-wired for private modern utility libraries and tests. Existing
production compatibility modules should still remain in place until the
filesystem pilot proves that the adapter approach can keep legacy behavior
stable.

Initial policy:

- keep public ABI headers in their current public/common/module homes
- keep compatibility modules buildable in place
- use this tree for new reusable implementation units once build integration is
  deliberate
- avoid dumping subsystem code here without a matching design note

Planned subfolders:

- `include/`: private reusable C++ headers for new internals
- `debugging/`: future shared debugging, snapshot, serialization, and trace
  utility implementation units
- `filesystem/`: future filesystem implementation units after the pilot
- `launcher/`: native launcher implementation, with executable entry glue in
  `launcher/platform/`
- `utilities/`: subsystem-neutral helper implementation units when a helper is
  not header-only

See also:

- `Documentation/codex/modularization-plan/cross-cutting-utilities.md`
