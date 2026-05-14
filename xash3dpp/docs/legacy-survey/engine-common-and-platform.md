# engine/common + engine/platform

## Purpose

These directories implement the host loop, command/cvar subsystems, console I/O, networking stack, memory management, and platform abstraction layer. They bridge the engine core with platform-specific implementations (Win32, POSIX, SDL, Android, iOS) and provide foundational services (messaging, configuration, networking) consumed by client, server, and renderer subsystems.

## Source Files

- **Host loop**: `host.c`, `host_state.c` — main frame loop, game state machine, shutdown handling
- **Command/cvar system**: `cmd.c`, `base_cmd.c`, `cvar.c` — command buffer, console variables, tokenization; `con_utils.c` — auto-completion, config I/O
- **Console/logging**: `sys_con.c` — console input/output, logging
- **Networking**: `net_ws.c` (sockets), `net_chan.c` (channels), `net_buffer.c` (message buffer), `net_encode.c` (encoding), `masterlist.c` (master server), `ipv6text.c`, `http/net_http_xash.c`
- **Memory**: `zone.c` — pooled allocation, memory tracking
- **Model/World**: `model.c`, `mod_studio.c`, `mod_alias.c`, `mod_bmodel.c`, `mod_sprite.c` — format loaders; `world.c`, `pm_trace.c`, `pm_surface.c` — physics traces
- **Image/Sound**: `imagelib/` (BMP, DDS, PNG, TGA, WAD, KTX2), `soundlib/` — media decoding
- **Platform abstraction**: `platform/platform.h`; subdirs `win32/`, `posix/`, `sdl2/`, `sdl3/`, `android/`, `ios/`, `misc/`, `stub/`
- **Miscellaneous**: `common.c`, `filesystem_engine.c`, `hpak.c`, `infostring.c`, `cfgscript.c`, `launcher.c`, `whereami.c`, `system.c`, `custom.c`, `sounds.c`, `dedicated.c`, `munge.c`, `library.c`, `lib_common.c`

## Key Data Structures and Globals

- `host` (host_parm_t) — monolithic state owner: timing, status, game state, mempools, frame count, config, keystate, feature flags
- `game_status_t` — state machine (init, load, changelevel, shutdown) and levelname tracking
- `netchan_t` — per-connection channel state: fragments, reliability, bandwidth flow stats
- `cvar_t` / `convar_t` — console variable linked list with change callbacks
- `cmd_t` — command function pointers and registry
- Memory pools (`mempool_t`) — `host.mempool`, `host.imagepool`, `host.soundpool` for lifetime scoping
- `soundlist_t`, `field_t` — active sound list, console input field

## Public Surface to Other Subsystems

- `Host_Frame()` — main per-frame entry point
- `Cmd_Argc/Argv/Args`, `Cbuf_AddText`, `Cmd_ExecuteString`
- `Cvar_Get/Set/VariableString/VariableValue/FindVar`
- `Mem_Alloc/Free/AllocPool`
- `NET_SendPacket/GetPacket`, `Netchan_*`
- `FS_LoadFile/Open/Search`, `FS_LoadImage/SaveImage`
- `Con_Printf/DPrintf`
- `Platform_*` — OS time, sleep, dialogs, shell execute

## Dependencies

- **filesystem/** — archive access
- **public/** — utility libs (crclib, crtlib, miniz, utflib, atlas, build info)
- **common/** — shared SDK structures
- **engine/eiface.h, engine/cdll_int.h** — game/client DLL interfaces
- **3rdparty/** — zlib, bzip2, libogg, opus, vorbis
- **ref/** — renderer plugin interface (via `ref_host_t`)
- **system/libc + OS APIs**

## Coupling and Risks

- **Giant `host_parm_t` global singleton** — 40+ fields, accessed everywhere; blocks refactoring into subsystem services
- **Pervasive macro-based platform abstraction** — `#ifdef XASH_WIN32`, `#ifdef XASH_SDL`, etc. thick throughout; hinders testing and cross-platform parity verification
- **Callback table explosion** — command functions registered as raw `xcommand_t` pointers; no lifecycle management; game DLL registration order implicit
- **Net channel fragmentation/reliability layer intertwined with winsock** — difficult to mock or replace transport; UDP-specific assumptions
- **Module format loaders intimately coupled to world/physics** — `pm_trace`, `mod_studio` callbacks; hard to decouple content pipeline

## Modernization Opportunities

- **Split `host_parm_t` into subsystems** — timing service, frame scheduler, game state machine, feature registry
- **Extract platform abstraction into a proper interface layer** — replace `#ifdef` soup with abstract OS backend; enable runtime backend selection and testing
- **Decouple command/cvar registration from global registry** — use context objects or scope-based registration
- **Isolate network transport from protocol encoding** — split socket I/O from `Netchan` logic; allow pluggable transport
- **Lazy-load or plugin model for content loaders** — move studio/alias format parsing into optional subsystem; share common trace/physics interface
