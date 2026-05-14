# Modern Common Contract Notes

This folder tracks modernization plans for the repository-root `common/`
headers.

The root `common/` folder is not simply an internal utility folder. It contains
SDK-facing ABI structures, renderer/client/sound callback tables, network
payload layouts, disk-format records, platform/build macros, and a smaller
amount of engine-owned helper data. Modernization should therefore add private
C++ concepts beside the legacy headers first, then route internals through
tested adapters only where layout and behavior are protected.

## Documents

- `header-ownership-audit.md`: Phase 167 classification of every root
  `common/*.h` header by compatibility role, fanout, and intended modern home.
- `structure-modernization-plan.md`: ownership classification, target source
  layout, and phase plan for modernizing `common/` without breaking legacy
  clients, renderers, game DLLs, or tools.
