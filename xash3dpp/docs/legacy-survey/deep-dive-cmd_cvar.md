# Deep Dive: Legacy Origins of `xash3dpp/cmd_cvar` — Command Buffer, Tokenizer, Cvar Registry, Aliases, base_cmd

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy behaviours the `xash3dpp`
**cmd_cvar** subsystem ports — the `cmd.c` command buffer + tokenizer, the
`cvar.c` console-variable registry + flag/wire semantics, the `cmd.c` alias
system, and the `base_cmd.c` unified autocomplete hash table — plus an honest
"As-built mapping" of what shipped, what diverged, and what is not yet ported.
Line numbers are against the working tree on that date; behaviour references,
not design constraints. Everything about the legacy is **reference-only**.*

Primary legacy sources:

- `engine/common/cmd.c` — command buffer (`cmd_text` / `filteredcmd_text`),
  `Cmd_TokenizeString`, the command registry, aliases, `wait`, `stuffcmds`,
  `exec`, `cmd_scripting` (`$cvar` substitution + `if`/`else`)
- `engine/common/cvar.c` — cvar linked list (`cvar_vars`), `Cvar_Get` /
  `Cvar_Set2` / `Cvar_DirectSet` / `Cvar_FullSet`, `Cvar_UpdateInfo`
  (userinfo/serverinfo side-effects), `Cvar_Unlink`, the `HACKS_RELATED_HLMODS`
  quirk sites, the HL25 `gl_widescreen_yfov` redirect
- `engine/common/base_cmd.c` / `base_cmd.h` — the unified sorted-bucket hash
  table (cvars + commands + aliases keyed by name, type-discriminated) that
  backs `Cmd_AutoComplete` / fast lookup — *"Inspired by Doom III"*
- `common/cvardef.h` — the frozen `cvar_t` (20/32 B) and engine `convar_t`
  layouts, the `CVAR_SENTINEL` magic, the `FCVAR_*` flag set

**Global assumptions (legacy):** all command/cvar state is **process-global**
(`static cmdbuf_t cmd_text;`, `static convar_t *cvar_vars;`, `static
base_command_hashmap_t *hashed_cmds[HASH_SIZE];`); there is no context object.
The engine assumes a single main thread runs the buffer, registers cvars, and
dispatches commands; DLL load/unload happen on that same thread.

______________________________________________________________________

## 1. Command buffer — `cmd.c` (the `cbuf_*` source material)

Two fixed 32 KB text ring buffers, drained once per frame:

| Legacy construct | Site | Meaning |
|------------------|------|---------|
| `#define MAX_CMD_BUFFER 32768` | `cmd.c:21` | fixed ring size, per queue |
| `#define MAX_ALIAS_NAME 32` | `cmd.c:23` | alias name cap |
| `cmdbuf_t { byte data[MAX_CMD_BUFFER]; int cursize; … }` | `cmd.c:25` | one ring buffer |
| `static cmdbuf_t cmd_text` | `cmd.c:32` | **trusted** queue (local console, `exec`, engine) |
| `static cmdbuf_t filteredcmd_text` | `cmd.c:33` | **unprivileged** queue (server `stuffcmd`) |
| `static int cmd_wait` | `cmd.c:31` | frame-skip counter for the `wait` command |

Key operations:

- **`Cbuf_AddText`** (`cmd.c:105`) appends to `cmd_text`; **`Cbuf_AddFilteredText`**
  (`cmd.c:127`) appends to `filteredcmd_text`; **`Cbuf_InsertText`** (`cmd.c:155`)
  *prepends* to `cmd_text` (used by `exec` so a config runs before queued text).
- **`Cbuf_Execute`** drains **both** queues each call:
  - `Cbuf_ExecuteCommandsFromBuffer( &cmd_text, true, -1 )` (`cmd.c:258`) — the
    trusted queue always runs privileged.
  - `Cbuf_ExecuteCommandsFromBuffer( &filteredcmd_text, SV_Active() &&
    SV_GetMaxClients() == 1, -1 )` (`cmd.c:268`) — **the stuffcmd queue runs
    privileged only on a local singleplayer listen server.** This inline test is
    the exact seam the rewrite replaces with `ITrustOracle::stuffcmd_is_trusted()`.
- **`wait`** (`Cmd_Wait_f`, `cmd.c:367`) sets `cmd_wait`; `Cbuf_Execute`
  decrements it once per call (`cmd.c:178`) and stops draining for that frame —
  one `wait` skips exactly one frame.
- Overflow of the 32 KB ring is a **fatal** `Host_Error` (the buffer is a fixed
  array; there is no growth path).

## 2. Tokenizer — `Cmd_TokenizeString` (the `tokenize_line` source material)

- **`#define MAX_CMD_TOKENS 80`** (`common.h:114`) — hard argv cap.
- Tokens are split on whitespace; `;` and `\n` are **command separators**
  (handled by the buffer executor, not the tokenizer); `//` starts a
  line comment.
- `"quoted strings"` are a single token; the legacy tokenizer does **not**
  treat backslash as an escape inside quotes.
- `Cmd_Argc` / `Cmd_Argv(i)` / `Cmd_Args()` expose the current token vector;
  `Cmd_Args()` is "everything after argv[0]" as a raw substring. These are valid
  only while a `CommandFn` is on the stack.
- **`cmd_scripting`** (`FCVAR_ARCHIVE|FCVAR_PRIVILEGED`, `cvar.c:22`): when set,
  the executor performs **`$cvar_name` substitution** (replaces the token with
  the cvar's current string before dispatch) and supports **`if` / `else`**
  conditional blocks. It is privileged so a remote server cannot use it to
  exfiltrate local cvar values via stuffcmd.

## 3. Cvar registry — `cvar.c` + `cvardef.h` (the `Cvar`/`CvarAbi` source material)

### 3.1 The two struct shapes (frozen ABI)

```c
// common/cvardef.h — DLL-facing, FROZEN.  STATIC_CHECK_SIZEOF(20, 32)
struct cvar_s   { char *name; char *string; int flags; float value; struct cvar_s *next; };
// engine-internal — adds default + description
struct convar_s { char *name; char *string; int flags; float value; struct convar_s *next;
                  char *desc; char *def_string; };
```

- Game/client DLLs register a short `cvar_t`; the engine defines the longer
  `convar_t`. The engine tells them apart with **`CVAR_SENTINEL`**
  (`0xDEADBEEF` / `0xDEADBEEFDEADBEEF`, `cvardef.h:109`) written into a known
  field, checked via `CVAR_CHECK_SENTINEL(var)` (`cvar.c:539`, `:712`). A cvar
  that fails the sentinel test *and* has `next == NULL` without
  `FCVAR_EXTENDED|FCVAR_ALLOCATED` is treated as a bare DLL `cvar_t`.
- `cvar_vars` (`cvar.c:20`) is the singly-linked list head, kept **sorted
  alphabetically by name** on insert (`cvar.c:464`, `:549`) — DLLs walk it via
  `Cvar_GetList`.

### 3.2 Registration / set / unlink

- **`Cvar_Get`** creates-or-returns; **`Cvar_Set2`** auto-creates a
  `FCVAR_USER_CREATED` cvar on a `set` to an unknown name (relied on by configs
  that set cvars before the owning DLL loads).
- **`Cvar_DirectSet`** (by pointer, bypasses privilege) and **`Cvar_FullSet`**
  (force value + flags, bypasses every guard including `FCVAR_READ_ONLY`) are the
  two escape hatches. `Cvar_FullSet` on an existing cvar **ignores** its flags
  argument (`cvar.c:775` passes `var->flags` back) and does **not** skip a
  same-value write.
- **`Cvar_Unlink`** / `Cvar_PrepareToUnlink`: on DLL unload, `def_string` is
  restored into `string`, `FCVAR_ALLOCATED` memory is freed, and the node is
  removed. Because a DLL may free its own `cvar_t` before telling the engine,
  a `pending_cvar_t` captures name+next first so cleanup is crash-safe.

### 3.3 Change side-effects — `Cvar_UpdateInfo` (`cvar.c:140`)

The write tail is where the **circular dependency** lives:

- `FCVAR_USERINFO` → `CL_Userinfo` / `CL_UpdateInfo` (client).
- `FCVAR_SERVER` (and not `FCVAR_UNLOGGED`) → `SV_Serverinfo` /
  `SV_BroadcastPrintf` (server).
- `FCVAR_CHANGED` is set on **every** write and cleared by consumers (renderer,
  physics) that *poll* it — a bitmask side-channel, not a callback.

### 3.4 The `FCVAR_*` flag set (frozen values, `cvardef.h`)

Bits 0–23 are public (compared by SDK-compiled game code); bits 24–30 are
engine-internal. The rewrite mirrors the numeric values exactly in
`CvarFlags` (`cvar.hpp`). Notable wire/behaviour flags: `FCVAR_ARCHIVE`
(persist), `FCVAR_USERINFO`/`FCVAR_SERVER` (info-string mirroring),
`FCVAR_PRIVILEGED`/`FCVAR_FILTERABLE` (stuffcmd trust), `FCVAR_CHEAT`
(gated on `sv_cheats`), `FCVAR_LATCH` (deferred to server restart),
`FCVAR_EXTENDED` (signals the long `convar_t` layout).

### 3.5 HL25 compat quirk

`Cvar_FindVar("gl_widescreen_yfov")` silently redirects to `r_adjust_fov`
(`cvar.c:81`). The rewrite moves this into the `ICompatPolicy` redirect table
(`compat_goldsrc.cpp`), one reviewed `constexpr` entry.

## 4. Aliases — `cmd.c` (the `AliasDef` source material)

`cmdalias_t` is a linked list of `{ char name[MAX_ALIAS_NAME]; char *value; }`.
The `alias` command creates/updates/lists; `unalias` removes. On dispatch, if
argv[0] matches an alias, the expansion string is re-fed to the buffer
(recursively, with a depth cap to stop `alias a "a"` loops). Aliases are checked
**before** the command registry.

## 5. base_cmd — the unified autocomplete table (`base_cmd.c`, *"Inspired by Doom III"*)

This is the piece most worth understanding because the rewrite **does not port
it yet**:

```c
#define HASH_SIZE 64                                   // base_cmd.c:20
struct base_command_hashmap_s {
    base_command_t         *basecmd;   // cvar | alias | command (void*)
    base_command_hashmap_t *next;
    base_command_type_e     type;      // HM_CVAR | HM_CMD | HM_CMDALIAS
    char                    name[];    // flexible key
};
static base_command_hashmap_t *hashed_cmds[HASH_SIZE]; // base_cmd.c:36
```

- **One** table holds cvars, commands, and aliases together, discriminated by
  `type` (`base_command_type_e { HM_DONTCARE, HM_CVAR, HM_CMD, HM_CMDALIAS }`,
  `base_cmd.h:21`).
- Each bucket chain is kept **sorted alphabetically** (`BaseCmd_FindInBucket`,
  `base_cmd.c:44`, breaks early on `cmp > 0`) — this is what makes
  `Cmd_AutoComplete` cheap: it walks names in order.
- `BaseCmd_Find` / `BaseCmd_Insert` / `BaseCmd_Remove` (`base_cmd.h:33–36`) are
  the surface; `Cmd_AddCommand` / `Cvar_RegisterVariable` / `alias` mirror every
  registration into this table.

______________________________________________________________________

## 6. As-built mapping (legacy origin → `xash3dpp/cmd_cvar`)

Honest ported-vs-diverged-vs-unbuilt accounting. "Ported" = behaviour carried
over; "Diverged" = same intent, different mechanism; "**Not yet built**" = the
legacy behaviour has no working code in the rewrite.

| Legacy construct | Site | `xash3dpp/cmd_cvar` as-built | Status |
|------------------|------|------------------------------|--------|
| `cmdbuf_t` fixed 32 KB rings ×2 | `cmd.c:25–33` | `std::deque<std::string> cmd_text` / `filteredcmd_text` (D5) | Diverged (heap deque, no fixed cap, informal `limits::cbuf_size`) |
| `Cbuf_AddText`/`AddFilteredText`/`InsertText` | `cmd.c:105–161` | `cbuf_add_text` / `cbuf_stuff_text` / `cbuf_insert_text`; `cbuf_split_push` splits on `;`/`\n` (quote-aware) | Ported |
| `Cbuf_Execute` two-queue drain + `wait` | `cmd.c:178,258,268` | `cbuf_execute()`; per-queue privilege from `trust_oracle`; `cmd_wait` frame gate | Ported |
| `SV_Active() && SV_GetMaxClients()==1` inline trust test | `cmd.c:268` | **`ITrustOracle::stuffcmd_is_trusted()`** (injected; no server include) — D2 | Diverged (decoupled) |
| `Cmd_TokenizeString` (`MAX_CMD_TOKENS 80`, quotes, `;`/`\n`, `//`) | `cmd.c` / `common.h:114` | `tokenize_line` in `cmd_dispatch.cpp`; argv cap `limits::cmd_tokens_max` | Ported |
| `cmd_scripting` `$cvar` substitution + `if`/`else` | `cmd.c`, `cvar.c:22` | cvar registered; `cmd_condition`/`condlevel` reserved; **no `$`-expansion or conditional parsing** | **Not yet built** (D9 pending) |
| `cl_filterstuffcmd` prefix filter + `FCVAR_FILTERABLE` | `cmd.c` `Cmd_ShouldAllowCommand` | cvar registered; `compat_policy->is_filterable_exempt` exists but **has no caller**; dispatch gates only `FCMD_PRIVILEGED` | **Not yet built** |
| `cvar_t` / `convar_t` + `CVAR_SENTINEL` detection | `cvardef.h`, `cvar.c:539,712` | `CvarAbi` (offset-0 in `Cvar`, `static_assert`) + **two explicit paths** `cvar_register_engine` / `cvar_register_dll` + `FCVAR_DLL_WRAPPER` — no sentinel heuristic (D4) | Diverged (cleaner) |
| `cvar_vars` sorted linked list | `cvar.c:20,464` | ABI `next` chain (unsorted, prepend) + separate `CmdHashMap<Cvar>` for lookup | Diverged (hash lookup; list not sorted) |
| `Cvar_Get`/`Set2` auto-create; `DirectSet`/`FullSet` | `cvar.c` | `cvar_get_or_create` / `cvar_set` / `cvar_set_direct` / `cvar_full_set` (legacy quirks preserved: FullSet ignores flags on existing, no same-value skip) | Ported |
| `Cvar_UpdateInfo` → `CL_*`/`SV_*` | `cvar.c:140` | **`ICvarObserver::on_cvar_changed`** (mask-filtered, injected) — D3; `FCVAR_CHANGED` still set for pollers | Diverged (decoupled) *(observer receives `def_string` as `old_value`, not the prior string — see boundary drift note)* |
| `Cvar_Unlink` / `pending_cvar_t` | `cvar.c` | `cvar_unlink` / `cvar_prepare_to_unlink` / `unlink_pending_cvars`; DLL-loaded guard | Ported |
| HL25 `gl_widescreen_yfov` → `r_adjust_fov` | `cvar.c:81` | `ICompatPolicy::redirect_cvar_name` `constexpr` table (D12) | Ported (isolated) |
| `HACKS_RELATED_HLMODS` exemptions / `CMD_OVERRIDABLE` | `cvar.c`/`cmd.c` | `compat_goldsrc.cpp` `kFilterableExemptions[15]` / `kOverridableCommands[5]` `constexpr std::array` (D12) | Ported (isolated); exemption list consulted only by `is_filterable_exempt`, whose caller is still unbuilt |
| `cmdalias_t` list + `alias`/`unalias` | `cmd.c` | `AliasDef { char name[alias_name_max]; char *value; }` + `alias`/`unalias` built-ins; alias checked before command; depth cap `k_alias_depth_max=8` | Ported |
| `stuffcmds` / `exec` | `cmd.c` | built-ins registered as **stubs** (`context_init.cpp:182,187`) awaiting host launch-args / filesystem VFile | **Not yet built** |
| `Cvar_WriteVariables` (`name "value"\n`) | `cvar.c` | **stub** (`cvar_ops.cpp:399`) awaiting filesystem VFile | **Not yet built** |
| `base_command_hashmap_t` unified **sorted** table + `BaseCmd_*` + `Cmd_AutoComplete` | `base_cmd.c` | `base_cmd.cpp` is a **placeholder TU** (static-asserts only); lookup uses three separate **unsorted** djb2 `CmdHashMap` chains — **no autocomplete surface** | **Not yet built** (console-UI-chunk concern) |
| built-in cmds `echo`/`wait`/`cmdlist`/`cvarlist` | `cmd.c` | registered in `context_init.cpp` (non-capturing lambdas reading `tls_ctx`) | Ported |

**Instrumentation added in the rewrite (no legacy analog):** per-cvar
`generation` atomic (lock-free change detection), `XASH_STATS`
`write_count`/`last_write_source`, `XASH_DEBUG_CVARS` `break_on_write` +
`hashstats`, and the `CvarWriteSource` / `CvarType` / `CvarDesc` / `CommandDesc`
introspection surfaces reserved for G-1 (MCP) and G-5 (scripting) — see
`docs/boundaries/cmd_cvar-boundary.md` *Extension axes (Q-21)*.
