# Modern Common Includes

This folder is reserved for private C++ views, value types, policies, and
compatibility adapter declarations that sit beside the legacy root `common/`
headers.

Do not move public SDK or ABI headers here. The root `common/*.h` files remain
the stable C-facing compatibility surface until a later decision explicitly
changes that boundary.

Expected future subfolders:

- `core/`: typed constants, bit helpers, byte-order helpers, and fixed-size
  value wrappers.
- `protocol/`: network-address, entity-state, event, movement, and user-command
  views.
- `assets/`: BSP, WAD, qfont, image, and model-format descriptors.
- `render/`: renderer, client, sound, and callback-table views.
- `compatibility/`: narrow C adapters for converting to and from legacy records.
