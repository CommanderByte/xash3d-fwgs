# engine/client

## Purpose

The client subsystem manages all client-side game state and rendering lifecycle, from network connection through game update/render. It owns command creation and network I/O, processes server messages (parse/), implements client-side movement prediction (`dll_int/cl_pmove.c`), renders the HUD/screen (console, `cl_scrn.c`), manages audio (`sound/`, `soundlib/`), input (`input/`), and bridges to the client DLL (`dll_int/`). Also handles demos (`cl_demo.c`), download queueing, temporary entities, and VGUI rendering.

## Source Files

- **Main loop & state**: `cl_main.c`, `client.h`, `cl_frame.c`
- **Network parse**: `parse/cl_parse.c`, `parse/cl_parse_gs.c`, `parse/cl_qparse.c`
- **Sound**: `sound/s_main.c`, `sound/s_load.c`, `sound/s_mix.c`, `sound/voice.c`, `soundlib/snd_*.c`
- **Input**: `input/input.c`, `input/in_*.c` (keys, joy, gyro, touch, OSK)
- **Prediction & pmove**: `dll_int/cl_pmove.c`, `cl_efrag.c`, `mod_dbghulls.c`
- **Screen/HUD/console**: `cl_scrn.c`, `console.c`, `cl_font.c`, `vgui/vgui_draw.c`
- **Client DLL bridge**: `dll_int/cl_game.c`, `dll_int/cl_gameui.c`, `dll_int/cl_render.c`, `dll_int/ref_common.c`
- **Demos & downloads**: `cl_demo.c`, `cl_custom.c`, `cl_video.c`, `cl_steam.c`
- **Effects & entities**: `cl_efx.c`, `cl_tent.c`, `cl_sprite.c`, `cl_events.c`, `cl_view.c`, `cl_remap.c`

## Key Data Structures and Globals

- `cl` (`client_t`) — per-level game state (frames, entities, models, player info, prediction state)
- `cls` (`client_static_t`) — persistent connection state (network, demo, UI, downloads, auth)
- `clgame` (`clgame_static_t`) — client DLL instance data (entities, sprites, events, messages)
- `gameui` (`gameui_static_t`) — MainUI DLL instance and state
- Sound state, input accumulation buffers, download queue `cls.dl`

## Public Surface to Other Subsystems

**Engine-facing**:

- `CL_Init/Connect/Disconnect/Frame` — main lifecycle
- `CL_Parse*` — network message routing
- `CL_WriteUsercmd` — encode client commands
- `SCR_UpdateScreen`, `Con_DrawConsole`, `S_*`
- State queries: `CL_IsInGame`, `CL_IsPlaybackDemo`

**Client DLL ABI** (`cdll_int.h`, `cdll_exp.h`):

- `pfnInitialize`, `HUD_VidInit`, `HUD_Redraw` — DLL lifecycle
- `CL_CreateMove`, `PlayerMove`, `CalcRefdef` — predict & camera
- `CL_DrawNormalTriangles`, `CL_DrawTransparentTriangles`
- `pfnProcessPlayerState`, `pfnTxferPredictionData`

## Dependencies

- **filesystem/** — pak/wad loading, demo I/O, downloads
- **ref/** — sprite/model loading, renderer (via `ref_common.c`)
- **public/** — common utilities
- **common/** — shared header contracts
- **engine/server/** — listen-server SVMOVE collision callbacks
- **engine/platform/** — SDL key/mouse events, clipboard
- **3rdparty/** — ogg, opus, vorbis

## Coupling and Risks

1. **Monolithic frame loop** — `CL_Frame()` sequences command → send → parse → predict → render with no clear phase boundaries
1. **Deep prediction coupling** — `cl_pmove.c` replicates server movement code and calls back into DLL for hull queries; any physics change breaks prediction
1. **Temporary entity system** — `cl_tent.c` owns a separate dynamic array parallel to `cl_entity_t`; diffuse spawning from parse/efx/events
1. **Demo file format lock** — encodes raw network deltas; changing entity state layout breaks all old demos
1. **Client DLL callback spaghetti** — function pointers everywhere; hard to mock/test without loading actual DLL

## Modernization Opportunities

- **Separate prediction module** — extract `PM_Move` and collision into a self-contained, versioned physics library
- **Event/entity streaming layers** — abstract stages (NetworkMessage → EntityDelta → LocalState) so each can be unit-tested
- **Scripted/native client DLL boundary** — replace callback function pointers with explicit message queues or FFI layer
- **Demo format versioning** — capability negotiation in demo header; support streaming/partial replay
- **Consolidate temp entity & effect spawning** — unify `cl_tent`, `cl_efx`, `cl_events` into single effect scheduler
