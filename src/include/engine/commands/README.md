# Engine Command Headers

Reserved for private command/cvar modernization headers.

Do not expose these through game, client, renderer, or filesystem ABI surfaces.

Current private headers:

- `base_command_registry.hpp`: modern helper for the legacy typed BaseCmd name
  table.
