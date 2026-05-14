# Client And Render Boundary Audit

Phase 159 looks outside the server migration lane and maps the next likely
modernization surface. This is an audit-only phase. It deliberately avoids
changing client prediction, renderer DLL calls, audio mixing, or client DLL
runtime ownership.

## Scope

The client/render side has several distinct boundaries that should not be
collapsed into one migration effort:

| Area | Files | Lines | Notes |
| --- | ---: | ---: | --- |
| `engine/client` recursive | 91 | 51452 | Full legacy client tree. |
| `engine/client` top level | 32 | 21001 | Core client state, console, screen, effects, demos, video, commands. |
| `engine/client/dll_int` | 7 | 7128 | Client DLL and renderer DLL bridges. |
| `engine/client/parse` | 3 | 3770 | Server message parsing and client frame mutation. |
| `engine/client/input` | 6 | 4162 | Keyboard, mouse, joystick, touch, gyro, OSK routing. |
| `engine/client/sound` | 8 | 5136 | Runtime sound channels, streaming, voice, DSP, mixer hooks. |
| `engine/client/soundlib` | 30 | 8682 | Audio format parsers and decoders. |
| `engine/client/avi` | 3 | 1019 | FFmpeg-backed AVI and startup video playback. |
| `engine/client/vgui` | 2 | 554 | VGUI draw bridge helpers. |
| `ref/common` | 5 | 946 | Renderer-common headers/helpers. |
| `ref/gl` | 24 | 21571 | OpenGL renderer implementation. |
| `ref/soft` | 22 | 16511 | Software renderer implementation. |

The scan confirms that the client side is not a single subsystem. It is a hub
where client DLL ABI, renderer DLL ABI, input, sound, AVI, VGUI, parsing, and
rendered console ownership all meet.

## Boundary Map

### Client DLL Bridge

`engine/client/dll_int/cl_game.c` is the client analog of the server game DLL
bridge. It owns client DLL load/unload, optional export discovery, VGUI startup
ordering, client callback table construction, render API setup, mobile API
setup, and sound API initialization.

This is a good audit target but a risky runtime target. The bridge exposes a
large C ABI to client DLLs, so the first modern work should describe metadata
and policy decisions rather than moving live callback wiring.

Low-risk candidates:

- client DLL export name and fallback policy;
- VGUI-before-client-DLL versus VGUI-after-client-DLL load ordering policy;
- callback table inventory documentation;
- client DLL load failure classification and user-facing reason strings.

Avoid for now:

- changing `gEngfuncs` layout;
- changing client DLL callback function addresses;
- changing `CL_LoadProgs` or `CL_UnloadProgs` lifetime behavior without a
  dedicated bridge fixture.

### Renderer DLL Bridge

`engine/client/dll_int/ref_common.c` owns renderer DLL selection and load
lifetime. It initializes renderer exports, renderer builtin textures, sky
setup, TriAPI-facing pieces, and renderer module selection.

Low-risk candidates:

- renderer name/config selection policy;
- renderer load failure classification;
- builtin texture name inventory;
- renderer capability snapshot values.

Avoid for now:

- renderer DLL function table mutation;
- `ref.dllFuncs` call routing;
- OpenGL or software renderer draw paths.

### Rendered Console

`engine/client/console.c` is the in-game rendered console. This is separate
from the system console work done earlier. It owns scrollback state, notify
lines, debug overlays, command history, background/font textures, input
history, and drawing through client/render helpers.

Low-risk candidates:

- scrollback admission and line wrapping fixtures;
- notify/debug line lifetime policy;
- console visibility state transitions;
- command history storage policy.

Avoid for now:

- draw calls;
- font texture ownership;
- console history filesystem writes;
- key destination changes until input fixtures exist.

### Screen, Video, And AVI

`cl_scrn.c`, `cl_video.c`, and `avi/avi_ffmpeg.c` tie screen update, loading
plaque, startup video, screenshots, cinematic playback, audio streaming, and
renderer texture upload together.

Low-risk candidates:

- startup cinematic list policy;
- screenshot/envshot request classification;
- AVI backend availability and load failure reasons;
- video queue selection and cancellation rules.

Avoid for now:

- FFmpeg decode loop;
- sound streaming callbacks;
- renderer texture upload and draw paths.

### Input

`engine/client/input` owns keyboard, mouse, joystick, touch, gyro, and on-screen
keyboard routing. It also forwards some events to VGUI and client DLL callbacks.

Low-risk candidates:

- key name/string/binding conversion fixtures;
- key destination routing decisions;
- mouse filter and grab option policy;
- touch button description and clamp policy.

Avoid for now:

- live input event dispatch;
- client DLL input callbacks;
- VGUI event calls;
- platform-specific grab behavior without platform fixtures.

### Sound

`engine/client/sound` owns sound channel runtime state, streaming, voice, DSP,
mixing hooks, ambient loops, and sound commands. `soundlib` owns format
parsers/decoders.

Low-risk candidates:

- channel admission and replacement policy;
- loop sample adjustment fixtures;
- sound name and sentence classification;
- raw channel selection rules;
- pure spatialization math where legacy fixtures are cheap.

Avoid for now:

- mixer/output ownership;
- streaming thread or callback behavior;
- decoder rewrites, especially in externally derived code, until licensing
  provenance is checked.

### Client Message Parsing

`engine/client/parse` mutates client frames, entities, deltas, downloads, and
client state based on server messages.

Low-risk candidates:

- message opcode classification;
- small byte-fixture parsers for headers and flags;
- parse error classification.

Avoid for now:

- frame mutation;
- entity interpolation state;
- prediction/runtime coupling.

### Menu And VGUI Interfaces

`engine/menu_int.h`, `vgui_api.h`, `cdll_int.h`, and `ref_api.h` are ABI
surfaces. `cl_gameui.c` and `vgui_draw.c` sit on the runtime side of that
boundary.

Low-risk candidates:

- menu/game UI load ordering policy;
- cursor/input routing classification;
- API inventory and compatibility notes.

Avoid for now:

- public struct layout changes;
- C++ types across module ABI;
- VGUI runtime startup changes without focused fixtures.

## Safe Next Candidates

These are analogous to the early server phases: value objects, policy helpers,
and fixtures that prove behavior before any route-through.

1. Client DLL bridge ABI inventory and load policy.
2. Rendered console scrollback and notify/debug line fixtures.
3. Input key binding and key destination policy fixtures.
4. Cinematic/AVI startup and backend availability policy.
5. Renderer loader selection policy and builtin texture inventory.
6. Sound channel admission and sound name classification fixtures.
7. Client message opcode classification fixtures.
8. Menu/VGUI bridge load ordering and input routing audit.

## Recommended Phase List

Do not start these automatically from Phase 159. Phase 160 should decide
whether to shift into this lane or continue server consolidation.

| Proposed Phase | Goal |
| --- | --- |
| Client DLL Bridge Audit And ABI Inventory | Document `cl_game.c` exports, callbacks, lifetime, and load failure behavior. |
| Rendered Console Fixture Baseline | Capture scrollback, notify/debug, visibility, and history behavior without touching draw calls. |
| Input And Key Binding Fixture Baseline | Capture key name, bind, and key destination behavior. |
| Cinematic And AVI Policy Baseline | Capture startup video, movie queue, and backend selection behavior. |
| Renderer Loader Boundary Audit | Map renderer DLL selection, builtin texture setup, and renderer load errors. |
| Sound Channel Policy Baseline | Capture channel admission, loop handling, and sound name classification. |
| Client Message Parser Fixture Baseline | Start with opcode/header fixtures before client frame mutation. |
| Menu And VGUI Bridge Boundary Audit | Map load order, cursor/input ownership, and ABI constraints. |
| Client/Render Consolidation Checkpoint | Decide whether any extracted pieces are ready for route-through or should remain fixtures. |

## Migration Guidance

The client/render lane should use the same compatibility-first method as the
server migration, but with a stricter runtime gate:

- build fixtures before route-through;
- keep C ABI surfaces C-owned;
- avoid renderer, prediction, sound mixer, and VGUI lifetime changes until
  there are targeted smoke tests;
- keep rendered console work separate from system console work;
- prefer cohesive subdomains over one tiny file per branch condition.

The likely first productive target is the rendered console fixture baseline,
because it has clear value behavior and high user visibility, while still
allowing draw calls and texture ownership to remain legacy-owned.
