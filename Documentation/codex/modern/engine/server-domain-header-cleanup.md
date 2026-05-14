# Server Domain Header Cleanup

This cleanup pass removes the flat forwarding headers that were left behind by
the server domain grouping phases.

## What Changed

The forwarding headers under `src/include/engine/server/*.hpp` for grouped
domains were deleted. Internal code and tests now include the canonical domain
paths directly:

- `engine/server/resources/...`
- `engine/server/messaging/...`
- `engine/server/game_dll/...`
- `engine/server/client/...`

The remaining flat headers in `src/include/engine/server/` are the headers that
still own their implementation at that level or have not yet been grouped into
a domain directory.

## Why Now

The forwarding headers were useful while Phases 137-140 moved files without
large include churn. After the Phase 148 aggregate tests and Phase 149 adapter
shrink, they were mostly visual noise: every forwarded include hid the actual
domain owner.

## Compatibility Boundary

These headers are part of the internal modern helper layer, not the engine's C
ABI. The cleanup does not alter legacy adapter entry points, exported symbols,
game DLL callbacks, packet formats, HPAK/filesystem ownership, or `server.h`.

External compatibility remains governed by the legacy C interfaces and adapter
headers in `engine/server/`.
