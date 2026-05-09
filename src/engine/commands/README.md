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
  command/alias/cvar name table. It is tested standalone and is not yet routed
  through `engine/common/base_cmd.c`.
