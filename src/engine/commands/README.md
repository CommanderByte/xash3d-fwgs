# Engine Commands

Reserved for modern helpers behind the legacy command and cvar system:

- `engine/common/base_cmd.c`
- `engine/common/cmd.c`
- `engine/common/cvar.c`
- related command completion behavior in `engine/common/con_utils.c`

Keep the existing C functions as the compatibility surface while extracting
small tested pieces behind them.

Current helper:

- `BaseCommandRegistry`: private C++ registry for the legacy `BaseCmd_*`
  command/alias/cvar name table. It is tested standalone and routed through the
  private `engine/common/base_cmd_adapter.cpp` bridge.
- `CommandBuffer`: private C++ byte queue for legacy-compatible `Cbuf_*`
  append, insert, overflow, and command-splitting mechanics. Dispatch policy
  remains in `engine/common/cmd.c`.
