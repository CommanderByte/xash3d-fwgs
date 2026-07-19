# Input Boundary Spec (Chunk 10)

> Assembled 2026-07-19 from A1 recon fragments R10.1 (keys/bindings),
> R10.2 (Platform_* input/window seam surface), R10.3 (joystick/gamepad +
> gyro), R10.4 (touch + on-screen-keyboard event model), R10.5 (usercmd
> frame drivers + full ABI slot list). Legacy sources: `engine/client/
> input/{input.c,in_keys.c,in_joy.c,in_gyro.c,in_touch.c,in_osk.c}`
> (~5,002 lines total per A0 fact base), `engine/platform/{platform.h,
> sdl2/{in_sdl2.c,joy_sdl2.c},linux/in_evdev.c}`, `engine/cdll_exp.h`,
> `engine/cdll_int.h`, `engine/menu_int.h`. This is a **pre-implementation
> draft** — not yet reconciled with any shipped code (0 TUs, per A0 census).
> Every load-bearing claim below carries its legacy `file:line` citation,
> lifted verbatim from the R10.* fragments; no citation was invented.

## Responsibility

The input module owns everything between a raw platform input event and a
per-frame movement/command contribution: keyboard scancode→binding
resolution and the `bind`/`unbind`/`resetkeys` config surface (in_keys.c,
5,002-line family, R10.1); the `key_dest`-routed event dispatch that decides
whether a keystroke goes to the game, the console, a chat message, or the
menu (`Key_Event`'s 13-step routing, R10.1 Quirks); joystick/gamepad axis
and button processing including trigger/deadzone math and the gamepad-gyro
calibration state machine (in_joy.c, joy_sdl2.c, R10.3); the built-in
device-gyro path (in_gyro.c, R10.3); the on-screen touch control surface —
button layout, edit mode, gesture recognition, and profile persistence
(in_touch.c, R10.4); the on-screen keyboard (in_osk.c, R10.4); and the
per-frame merge of all these sources into the outgoing `usercmd_t` via
`IN_EngineAppendMove`/`IN_Commands` (input.c, R10.5). It also owns the
mouse-activation state machine (grab/relative-mode/cursor visibility,
R10.5) and drives the client-DLL and menu-DLL ABI slots that carry input
events across those two boundaries.

It does **not** own: window creation or the render surface (renderer/
refresh boundary — `R_Init_Video`/`VID_SetMode`/`GL_*`/`SW_*`/
`R_GetWindowHandle`/`VID_Info_f`, all non-`Platform_`-prefixed, R10.2
out-of-scope table); dedicated-server console stdin (`Platform_Input`,
routed through `host.c:489`'s frame pump, not input core, R10.2:70-71);
command/cvar registration machinery itself (`cmd_cvar` owns `Cmd_AddCommand`/
`Cbuf_*`, per sibling-scope rule — input only *calls* it); or file I/O for
config/profile persistence (`filesystem` owns `FS_Delete`/`FS_Rename`/
`FS_LoadFile`, touch.cfg and `gamecontrollerdb.txt` load through it,
R10.3:75, R10.4:63-64).

## External ABI contracts

Input drives three frozen inter-DLL vtables. None of these are vendored in
Chunk 10 — this section records the **Chunk-12 vendoring targets** the
internal `Input` class surface must be able to back later; today's work is
internal-only.

### 1. `cldll_func_t` (client DLL exports — engine calls into client)

`CLDLL_INTERFACE_VERSION 7` (cdll_int.h:311, R10.5). Ordering is
load-bearing ("NOTE: ordering is important!", cdll_exp.h:49, R10.5) — this
is an ABI-versioned vtable, not a named-lookup table.

| Slot | Signature | Legacy caller in input core |
|---|---|---|
| `IN_ActivateMouse` | `void(void)` | `IN_ActivateMouse` (input.c:302-311) |
| `IN_DeactivateMouse` | `void(void)` | `IN_DeactivateMouse` (input.c:320-329) |
| `IN_MouseEvent` | `void(int mstate)` | mouse-range key synthesis in `Key_ClearStates` (in_keys.c:918-940) |
| `IN_ClearStates` | `void(void)` | `Key_ClearStates` tail (in_keys.c:918-940) |
| `IN_Accumulate` | `void(void)` | (declared; no input-core call site cited) |
| `CL_CreateMove` | `void(float frametime, usercmd_s* cmd, int active)` | `CL_CreateCmd` (cl_main.c:698-701) — runs **before** `IN_EngineAppendMove` |
| `KB_Find` | `void*(const char* name)` | opaque, caller-cast (cdll_exp.h:80-82) |
| `pfnKey_Event` | `int(int down, int key, const char* binding)` | `Key_Event` step 5 first-refusal (in_keys.c:734-750) |
| `pfnTouchEvent` (FWGS ext.) | `int(int type, int fingerID, float x, float y, float dx, float dy)` | `IN_TouchEvent` step 5 (in_touch.c:2172-2187) |
| `pfnMoveEvent` (FWGS ext.) | `void(float forwardmove, float sidemove)` | `IN_Commands` modern path (input.c:617-634) |
| `pfnLookEvent` (FWGS ext.) | `void(float relyaw, float relpitch)` | `IN_Commands` modern path (input.c:617-634); its presence is the "modern path" opt-out switch for `IN_EngineAppendMove` (input.c:587-591) |

Touch/Move/Look are explicitly "Xash3D FWGS extension" — not part of the
original GoldSrc surface (cdll_exp.h:80-82, R10.5).

### 2. `cl_enginefunc_t` (engine exports — client DLL calls into engine)

| Slot | Signature | Legacy impl |
|---|---|---|
| `Key_Event` | `void(int key, int down)` | direct call into `Key_Event` (cdll_int.h:187-189) |
| `GetMousePosition` | `void(int* mx, int* my)` | (cdll_int.h:187-189) |
| `Key_LookupBinding` | `const char*(const char* pBinding)` | = `Key_KeynumToString(Key_GetKey(pBinding))` (in_keys.c:326-334, cdll_int.h:219) |
| `pfnGetMousePos` | `void(tagPOINT* ppt)` | real forwarder to `Platform_GetMousePos` (cl_game.c:2900-2911) |
| `pfnSetMousePos` | `void(int x, int y)` | wired directly to `Platform_SetMousePos` (cl_game.c:3865-3867) |
| `pfnSetMouseEnable` | `void(qboolean fEnable)` | **the one** verified dinput-legacy stub, empty body: `/* legacy of dinput code */` (cl_game.c:2913-2922). **Correction**: the brief's "two dinput stubs" is not corroborated by R10.5's repo-wide grep — only this single slot carries that comment; treat as one, not two. |

### 3. `ui_enginefuncs_t` / `UI_FUNCTIONS` (menu DLL ABI, menu_int.h)

| Direction | Slot | Notes |
|---|---|---|
| engine→UI | `pfnKeyClearStates` | called on menu open/close (menu_int.h:59-180) |
| engine→UI | `pfnSetKeyDest(int dest)` | |
| engine→UI | `pfnKeynumToString` / `pfnKeyGetBinding` / `pfnKeySetBinding` / `pfnKeyIsDown` | thin wrappers over the in_keys.c primitives |
| engine→UI | `pfnKeyGetOverstrikeMode` / `pfnKeySetOverstrikeMode` | |
| engine→UI | `pfnKeyGetState` | "for mlook, klook etc" |
| UI→engine | `pfnKeyEvent(int key, int down)` | routed via `UI_KeyEvent`, gated on `gameui.hInstance` (cl_gameui.c:67-77) |
| UI→engine | `pfnMouseMove(int x, int y)` | routed via `UI_MouseMove`, called unconditionally from `IN_MouseMove` alongside `VGui_MouseMove` (input.c:355-358) |
| UI→engine | `pfnGetCursorPos` / `pfnSetCursorPos` / `pfnShowCursor(int)` | |
| UI→engine | `pfnCharEvent(int key)` | routed via `UI_CharEvent`, gated on `gameui.hInstance` (cl_gameui.c:131-135) |
| UI→engine | `pfnMouseInRect(void)` | |

### Compat scope (Q-12)

The compat surface this subsystem must preserve byte/string-for-string is
**not** a wire format (input has no netchan payload of its own) — it is:

- **Key codes and names**: the `keynames[]` table (in_keys.c:45-159, ~130
  rows) is the config-compat surface — every `bind`/`unbind`/`resetkeys`
  invocation and every written `config.cfg` line depends on these exact
  name strings and keynum values being stable across the rewrite.
- **`config.cfg` bind-line format**: `Key_WriteBindings` emits
  `bind "<name>" "<escaped-binding>"`, always double-quoted "for compat
  with mods that regex-parse config.cfg" (in_keys.c:462-484) — this quoting
  convention is a documented external compat requirement, not incidental
  style.
- **`touch.cfg` / profile format**: a sequence of `touch_*` console
  commands (not a data serialization format), fixed emission order: cvars,
  global stroke, `touch_setclientonly 0`, `touch_removeall`,
  `touch_aspectratio`, then one `touch_addbutton` line per surviving
  user button, `TOUCH_FL_CLIENT` buttons skipped (in_touch.c:250-307,
  R10.4). The write-new/rotate-backup file dance (`.new`/`.bak`) is
  `filesystem`'s I/O surface, not input's, but the **command-script
  format itself** is input's compat contract.
- **`gamecontrollerdb.txt` / `controllermappings.txt`**: loaded via
  `FS_LoadFile` (filesystem-owned) then handed verbatim to
  `SDL_GameControllerAddMappingsFromRW` (joy_sdl2.c:140-155, R10.3) — the
  file *format* is SDL's community mapping DB syntax, not input's to
  redefine, but the two-file load-order (base DB then overrides) is
  input's own compat contract.

No `cdll_exp.h`/`cdll_int.h`/`menu_int.h` slot signatures are vendored by
Chunk 10 — they are recorded above purely as the shape the eventual Chunk-12
ABI shim must reproduce.

## Interface (what the rest of the engine calls)

The planned `Input` class surface splits into an event/query source and a
window-property mutator, per R10.2's two-column partition analysis (its
`needs hWnd` / `needs SDL_INIT_VIDEO` / `refState scaling` rationale keys
are reused verbatim below).

### `IEventSource` (typed events + polled queries; no window-property mutation)

| Member | Legacy backing | Notes |
|---|---|---|
| event pump | `Platform_RunEvents` (`SDL_PollEvent` loop, host_sdl2.c:426-436) | needs `SDL_INIT_VIDEO` for the queue to be populated; does not mutate window state |
| `pointer_delta()` **POLLED** | `Platform_MouseMove` (`SDL_GetRelativeMouseState`, in_sdl2.c:72-78) | **Record**: R10.5's `IN_EngineAppendMove` finding — the *legacy-path* engine move-merge (no `pfnLookEvent` client DLL) calls `IN_CollectInput(..., includeMouse=false)` (input.c:599-614) and **never reads mouse deltas at all** for that path; mouse look for legacy clients is handled entirely inside the client DLL's own code via the ABI exports, not via this engine-side POLLED accessor. The modern path (`pfnLookEvent` present) *does* read it, gated `in_mouseinitialized && !m_ignore.value` (input.c:617-634). |
| `mouse_pos()` | `Platform_GetMousePos` (GAME_EXPORT, `refState.scale_x/y`-scaled, in_sdl2.c:44-53) | scaling contract must survive — VGUI/`cl_game.c:2907` reads scaled coords |
| `key_modifiers()` | `Platform_GetKeyModifiers` (`SDL_GetModState`, in_sdl2.c:261-290) | no window dependency; no call site found in input core proper (R10.2 med-confidence "unreferenced") |
| joystick lifecycle probe | `Platform_JoyInit`/`Platform_JoyShutdown` return/count semantics | count is enumerable-devices-at-init, NOT a success bool, NOT opened-controller count (joy_sdl2.c:371-398, R10.3) |
| gamepad-gyro calibration trigger | `Platform_CalibrateGamepadGyro` | trampoline to `SDLash_RestartCalibration` |
| haptics | `Platform_Vibrate`/`Platform_Vibrate2` | output to an input device; negative args mean "randomize," not "off" (joy_sdl2.c:334-352) |
| evdev alt-backend | `Evdev_Init/Shutdown/SetGrab`, `IN_EvdevMove`, `IN_EvdevFrame` | Linux raw-input, runs *alongside* SDL at the same call sites, not a replacement (input.c:186,194,562,620) |
| text-input mode toggle | `Platform_EnableTextInput` | event-stream mode switch (SDL IME), not a window property |
| bindings snapshot | new — see below | typed introspection surface, P-4 |

`Platform_SetTimer` stays **platform**'s surface (Linux-only frame-timer
signal, no SDL/window/input-device character — R10.2 Uncertainties flags
this explicitly for the sibling-scope rule; adjudicated here as platform's
time-ownership, consistent with CLAUDE.md's "platform owns time/sleep/...").
`Platform_Input` (dedicated-server stdin) is likewise **out of input
scope** — it is a `Host_Frame` command-pump consumer (host.c:489), not
game input; it belongs wherever dedicated console text lives (platform's
existing console ownership).

### `IWindowControls` (window-property mutators — null-backed stub, Chunk-12 vendoring target)

Per R10.2's partition test, only **4** functions genuinely dereference
`host.hWnd`. The class is null-backed today (no window in scope for this
chunk) and carries the full list below so a Chunk-12 real backend has a
fixed contract to fill:

| Member | Legacy backing | Needs hWnd? |
|---|---|---|
| `set_mouse_pos(x,y)` | `Platform_SetMousePos` → `SDL_WarpMouseInWindow(host.hWnd,...)` | **yes** |
| `get_mouse_grab()` | `Platform_GetMouseGrab` → `SDL_GetWindowGrab(host.hWnd)` | **yes** |
| `set_mouse_grab(bool)` | `Platform_SetMouseGrab` → `SDL_SetWindowGrab(host.hWnd,...)` | **yes** |
| `minimize()` | `Platform_Minimize_f` → `SDL_MinimizeWindow(host.hWnd)` (guarded `if(host.hWnd)`) | **yes** |
| `set_cursor_type(type)` | `Platform_SetCursorType` — sets `host.mouse_visible`, warps via `SetMousePos`, `SDL_ShowCursor`/`SDL_SetCursor` | transitively yes; also touches `host.window_center_x/y` |
| `get_clipboard_text()` / `set_clipboard_text()` | `Platform_GetClipboardText`/`SetClipboardText` (SDL clipboard, process/WM-scoped) | no hWnd param, but needs `SDL_INIT_VIDEO` (R10.2 low-confidence on the VIDEO requirement specifically) |

Adjudicated (interface segregation, 2026-07-19): joystick lifecycle,
gyro calibration, key modifiers, haptics, `pre_create_move`, and the evdev
seam are **device-side surface and live on `IEventSource`** (their rows
above) — a window-property interface must stay window-only, since a future
composite/replay event source could never meaningfully forward `grab()`.
`IWindowControls` is exactly the hWnd-dereferencing set plus the cursor
composite; clipboard placement is INP-OQ-3 (pending the window decision,
carried here null-backed in the interim).

`R_GetWindowHandle`, `VID_Info_f`, and all non-`Platform_`-prefixed
window-management functions are **out of scope entirely** — renderer/
refresh boundary (R10.2 out-of-scope table).

`Platform_Input` (dedicated console text) is **out of input scope** per
above — explicitly not part of either interface.

### Bindings, key-dest, and cvar/command surface

| Member | Legacy backing |
|---|---|
| `bindings_snapshot()` | typed read of `keys[265]`'s `binding`/`down`/`gamedown`/`repeats` fields (in_keys.c:22-43) — P-4 introspection surface, replaces raw array access |
| `key_dest` accessor | `Key_SetKeyDest`/`cls.key_dest` (in_keys.c:883-911) |
| `is_down(keynum)` | `Key_IsDown` (in_keys.c:168-173) |
| `string_to_keynum` / `keynum_to_string` | `Key_StringToKeynum`/`Key_KeynumToString` (in_keys.c:188-254) — **the static `tinystr[16]` return buffer must not survive the port** (Owned state below) |
| `set_binding` / `get_binding` / `lookup_binding` | `Key_SetBinding`/`Key_GetBinding`/`Key_LookupBinding` (in_keys.c:261-334) |
| `write_bindings(file)` | `Key_WriteBindings` (in_keys.c:462-484) |
| commands: `bind`/`unbind`/`unbindall`/`resetkeys`/`bindlist` | in_keys.c:341-503 — restricted (trust-gated) vs unrestricted split, see Quirks |
| ~24 `touch_*` commands (9 unrestricted, 15 restricted) | in_touch.c:1134-1157, R10.4 census |
| 14 `touch_*` cvars + `touch_enable` | in_touch.c:1160-1187 |
| ~26 `joy_*`/`gyro_*` cvars | in_joy.c:48-72, in_gyro.c:20-27, R10.3 |
| `key_rotate` cvar | in_keys.c:161 |

## Dependencies (what this module calls)

| Dependency | Used for |
|---|---|
| `cmd_cvar` | Command registration (`Cmd_AddCommand`/`Cmd_AddRestrictedCommand` for bind/unbind/touch_*/joy commands), `Cbuf_AddTextf` (touch config re-exec), cvar registration for every `joy_*`/`gyro_*`/`touch_*`/`key_rotate`/`m_ignore` cvar, and `Cmd_ExecuteString` (`IN_JoyAppendMove`'s synthetic `+forward`/`-forward`/etc. commands, input.c:552-578) |
| `platform` (window/event seam, split by R10.2) | `IEventSource`/`IWindowControls` backing per above — no *new* platform ownership; input consumes platform's existing SDL/evdev seam, it does not own window/time/sleep/dynlib/paths/console/crash/sockets |
| `filesystem` | `FS_LoadFile` for gamepad mapping DBs (joy_sdl2.c:140-155); `FS_Delete`/`FS_Rename`/`FS_FileExists` for touch profile persistence (in_touch.c:316-339, 1044-1054) — sibling-owned, input only calls it |
| the `pfnLookEvent` bypass seam | `IN_EngineAppendMove` fully no-ops when the client DLL implements `pfnLookEvent` (input.c:587-591) — this is not a dependency on another *subsystem* but a documented internal branch-point that determines whether the legacy engine-side merge math (viewangle clamp, `cl.viewangles` mirroring) runs at all; the rewrite's `Input` class must expose both code paths behind this same switch, not collapse them |
| VGUI (`VGui_MouseMove`/`VGui_MouseEvent`/`VGui_KeyEvent`/`VGui_IsActive`/`VGui_UpdateInternalCursorState`) | fires unconditionally alongside key/mouse/touch routing (in_keys.c step 8; in_touch.c:2172-2187) — VGUI is a client-side consumer, not owned by input, but input calls into it at fixed points in the routing order |
| client-DLL/menu-DLL ABI slots (External ABI contracts above) | the routing endpoints for game/console/menu dispatch |

## Owned state

| State | Legacy backing | Notes |
|---|---|---|
| `keys[265]` (`enginekey_t`: binding ptr, `down:1`, `gamedown:1`, `repeats:30`) | in_keys.c:22-43 | file-scope static; deliberately over-provisioned vs. ~255 real keys, +9 international slots |
| `keynames[]` (~130 rows, name/keynum/default-bind) | in_keys.c:45-159 | static const; drives lookup, `Key_Init`'s initial binds, `resetkeys`'s full-default replay |
| static `tinystr[16]` return buffer in `Key_KeynumToString` | in_keys.c:229 | classic C-ABI reuse hazard — **must not survive the port** as a shared mutable buffer; becomes a typed return value |
| `key_rotate` cvar | in_keys.c:161 | |
| `cls.key_dest` (external `keydest_t`) | in_keys.c:560 (client.h) | owned by the broader client state, input reads/writes it but does not declare it — treat as a dependency-in, not owned-here, in the final class design |
| `touch` struct (single file-scope, button lists + gesture state + move accumulator + edit cursor + config) | in_touch.c:90-137 | `touch.list_user`/`touch.list_edit` intrusive doubly-linked lists (O(n) scan, not arrays), `touch.mempool` pool-owned (`Mem_AllocPool("Touch")`) |
| `g_DefaultButtons[]`/`g_DefaultButtonsLength` (game-DLL-registered templates) | in_touch.c:142-143 | separate realloc'd array, distinct from `touch.list_user` |
| `osk` struct (enable flag, layout index, shift flag, `sending` re-entrancy guard, cursor position) | in_osk.c:80-92 | |
| `osk_keylayout[2][4]` fixed compile-time table | in_osk.c:64-78 | 7-bit ASCII only; Russian variants dead-code-commented-out |
| `joyaxis[]`/`joyaxesmap[]` (hardware→engine axis binding) | in_joy.c | written by the SDL event handler, read by `Joy_FinalizeMove` |
| `gyrocal` module-static (gyro calibration state machine) | joy_sdl2.c:67-138 | |
| `joy_gyro_speed`/`joy_gyro_speed_display`, `gyro_speed` | in_joy.c:299-303, in_gyro.c:38-48 | display buffer survives the per-frame clear; live buffer does not |
| ~26 `joy_*`/`gyro_*` cvars, 14 `touch_*` cvars + `touch_enable`, `key_rotate` | in_joy.c:48-72, in_gyro.c:20-27, in_touch.c:1160-1187 | full census in Interface above |
| module statics `in_mouseactive`, `in_mouseinitialized`, `in_lastvalidpos`, `in_mouse_savedpos`, `in_mstate`, `inputstate` (lastpitch/lastyaw) | input.c:29-40 | mouse activation state machine |
| function-local edge latches `s_bRawInput` (`IN_SetRelativeMouseMode`), `s_bMouseGrab` (`IN_SetMouseGrab`) | input.c:214,250 | Race-static-buf shape, see Threading |
| function-local `static uint moveflags` in `IN_JoyAppendMove` | input.c:480 | persists across frames; must become explicit owned state if collect/append ever split onto different call sites |
| `clgame.client_dll_uses_sdl` | cl_game.c:4018-4019 | set once at client-DLL load, read every frame |

## Quirks and invariants

1. **`Key_Event`'s 13-step routing order** (in_keys.c:709-855, R10.1,
   verbatim step list) must be preserved exactly: `Key_Rotate` remap →
   OSK first refusal (absolute — before even `keys[key].down` is
   written) → stale key-up guard → cinematic filter → client-DLL first
   refusal (`down || gamedown`, inverted-sense return, up events still
   routed if `gamedown` was set) → autorepeat accounting/suppression →
   unbound-key console warning → unconditional `VGui_KeyEvent` → console-key
   hardcode (` ` `/`~`) → ESC special-case (only in `key_game`) → menu-dest
   char synthesis + `UI_KeyEvent` → key-up-only short-circuit
   (`Key_AddKeyCommands` only, even in console/menu/message mode) →
   final key-down dispatch by `key_dest`.
2. **ESC is the only permanently-protected key**: `unbind` refuses it
   outright ("Can't unbind ESCAPE key", in_keys.c:341-366).
3. **`unbindall` ⊊ `resetkeys`**: `unbindall` clears then re-applies
   exactly two hardcoded defaults (`K_ESCAPE`/`K_START_BUTTON` →
   `cancelselect`, in_keys.c:373-386); `resetkeys` clears then replays the
   **entire** `keynames[]` table (in_keys.c:393-407).
4. **`bind`/`unbind`/`unbindall`/`resetkeys` are trust-gated
   (`Cmd_AddRestrictedCommand`); `bindlist`/`makehelp` are not**
   (in_keys.c:575-579).
5. **`Key_ClearStates` skips entirely when `cls.changelevel` is set** —
   held keys deliberately survive a level transition; otherwise it
   replays releases through the *real* event path (`IN_MouseEvent`/
   `Key_Event`, not raw field clears) before force-zeroing bookkeeping
   (in_keys.c:918-940).
6. **Joystick lazy re-parse**: `joy_axis_binding` is only re-parsed inside
   `Joy_FinalizeMove`, gated on `FCVAR_CHANGED`, once per frame — never at
   init or via a cvar callback; the very first frame uses whatever
   static-init left in `joyaxesmap[]` (in_joy.c:317-337).
7. **Gyro calibration self-latches and runs forever in the background**:
   `SDLash_FinalizeCalibration` re-arms another 5s window on every success
   and silently re-averages continuously; a failed *continuous* re-cal is
   muted (no state change), only the first calibration can transition to
   `JOY_FAILED_TO_CALIBRATE` (joy_sdl2.c:67-138).
8. **`joy_axis_binding` doc-string bug**: the cvar's help text labels
   `r`/`l` as "left trigger"/"right trigger" but the switch statement maps
   `'r'→JOY_AXIS_RT`, `'l'→JOY_AXIS_LT` — textually inverted relative to
   the axis-constant names' own comments (`JOY_AXIS_RT // right trigger`).
   Flagged as a genuine legacy doc bug, not a behavioural quirk to
   reproduce in prose (in_joy.c:60-61, R10.3 Uncertainties).
9. **Touch gesture static-accumulator hack**: two function-static floats
   (`x1`, `y1`) inside `IN_TouchEvent`'s console/message branch persist
   swipe distance across calls, self-described in-source as "absolutely
   horrible" — drive both an immediate big-swipe scroll and an
   accumulated page-up/down repeat-fire off the same `dy` sum
   (in_touch.c:2107-2157).
10. **OSK gets absolute first refusal** in `Key_Event`, before the
    stale-release guard and before `keys[key].down` is even latched
    (in_keys.c:713-716) — but `OSK_KeyEvent` is a no-op passthrough unless
    both compiled/enabled and `osk_enable` is set, so this is dead weight
    on desktop builds by default (in_osk.c:94-99).
11. **`IN_LockInputDevices` READ_ONLY flip**: mutates `FCVAR_READ_ONLY`
    bitfields directly on `m_ignore`, `joy_enable` (extern, private to
    input.c), and `touch_enable` — a pure flag toggle, fully reentrant/
    idempotent, used to freeze the device-connect surface once a player
    is connected (input.c:92-108).
12. **Legacy-path engine move-merge never reads mouse deltas**: when the
    client DLL does *not* implement `pfnLookEvent`, `IN_EngineAppendMove`
    calls `IN_CollectInput(..., includeMouse=false)` (input.c:599-614) —
    the engine-side merge for that path is joystick + touch + gyro only;
    mouse look is entirely the client DLL's own responsibility via the
    ABI exports. **This corrects the brief's implicit assumption** that
    engine move assembly always consumes mouse deltas.
13. **`CL_CreateCmd` order is fixed**: `Platform_PreCreateMove()` →
    `clgame.dllFuncs.CL_CreateMove(...)` → `IN_EngineAppendMove(...)` —
    the engine merge always runs *after* the client DLL's own move build,
    into the same `cmd` (cl_main.c:698-701).
14. **Prefix-match, not exact-match, binding lookup**: `Key_GetKey`
    (backing `Key_LookupBinding`) does a case-insensitive *prefix*
    compare, first match wins in keynum order (in_keys.c:294-318).
15. **`bindlist` and `Key_WriteBindings` diverge**: `bindlist` prints raw,
    unescaped, no `unbindall` header; `Key_WriteBindings` always escapes
    and always double-quotes for mod-compat (in_keys.c:462-503).

## Satellite components

Q-11 test (own protocol / own state machine / own external dep / useful
standalone / small interface to parent) applied per candidate:

| Candidate feature | Criteria met | Score | Verdict |
|---|---|---|---|
| **Touch controls (in_touch.c)** | (a) independent state machine (edit/edit_move), own config format (touch.cfg), own ~24-command surface, own gesture accumulator | 3 | leans **separate**, but shares the exact `IEventSource`/dispatch target with keys/joy (all funnel into the same `Key_Event`/`Touch_GetMove`→`IN_CollectInput` merge) — same-target recommendation below |
| **On-screen keyboard (in_osk.c)** | (a) independent state machine (layout/shift/cursor), small interface to parent (`OSK_KeyEvent` intercepted at the top of `Key_Event`, `OSK_EnableTextInput` substitutes for platform IME) | 2 | **same target** — `osk.enable`/`osk_enable` gate is a runtime kill switch, not a build-time boundary; OSK's only forward path is `CL_CharEvent`/`Key_Event`, i.e. it is a keyboard-input *alternate source*, not a standalone service |
| **Touch + OSK combined verdict** | both score low on "useful without parent" (touch's move output feeds the same `IN_CollectInput` pipeline as joystick/gyro; OSK's char output feeds the same console/menu dispatch as physical keys) and both fail "own external dep" (no dep beyond what `IEventSource`/`filesystem` already provide) | — | **same target** as the rest of input — likely the same recommendation the joystick/gyro pairing gets (both score similarly: independent axis-processing state machine, but zero external-dep or standalone-usefulness criteria met) |
| **Joystick/gamepad (in_joy.c, joy_sdl2.c)** | (a) independent calibration state machine (gyro), SDL-specific backend file | 1-2 | **same target** — feeds the identical `IN_CollectInput` merge point as touch/gyro; no standalone use case |
| **Device gyro (in_gyro.c)** | (a) independent state (orientation-aware rotation), but structurally near-identical math to gamepad-gyro, shares `IN_CollectInput` call site | 1 | **same target** |

**Net verdict**: none of touch/OSK/joystick/device-gyro clears the Q-11
bar for a separate CMake target — every candidate's output funnels into
the same `IN_CollectInput`/`Key_Event` dispatch points and none has an
external dependency beyond what the shared `IEventSource`/`filesystem`
seams already provide. All stay inside a single `xash3dpp_input` target,
organized as sibling source files (keys/, joy/, gyro/, touch/, osk/)
mirroring the networking-boundary precedent's subfolder-at-scaffold-time
lesson.

## Extension axes (Q-21)

Axis set re-read live from `extension-goals.md` §2/§3 at assembly time
(2026-07-19): G-1..G-5, P-1..P-8.

| Goal / primitive | Applies? | Required seam or door |
|---|---|---|
| **P-4** Typed introspection surfaces | **Yes** | `bindings_snapshot()` replaces raw `keys[265]` array access; `key/key_dest` state gets typed accessors (Interface section) rather than an `extern` poke. Any future debug overlay of active bindings/touch layout extends this snapshot, never reaches into `Impl`. |
| **G-5** Scripting runtime / **G-1** MCP service (synthetic-event injection door) | **Yes** | `IEventSource` is the natural injection seam: a `MockEventSource` (or a future scripting/MCP driver) implements the same interface a real SDL backend does and feeds synthetic key/mouse/touch events through the identical `Key_Event`/`IN_TouchEvent` dispatch path. Commands (`bind`, `touch_*`, `joy_*` cvars) are already reachable via `cmd_add`/`cvar_*` per extension-goals §G-5's existing script-surface-v0 list — no new door needed there, input just needs to not invent a private bypass around `cmd_cvar`. |
| **P-3** Context-first entry points | **Yes — door-debt today, closes at port time** | Legacy is almost entirely file-scope statics (`keys[265]`, `touch`, `osk`, `joyaxis[]`, module statics in input.c). The rewrite's `Input` class must hold these as members, not globals — this is the single largest P-3 gap in the surveyed material (R10.4's own Uncertainties flags this for touch/OSK explicitly). No exception is warranted; this is ordinary Q-2/P-3 context-object work, not an ABI-forced global. |
| **P-2** Published-snapshot reads | Door-keep, not yet a consumer | No current off-main reader exists (input is `T_Main`-only per the ratified model). If G-3/G-1 ever wants live input-state readout off-main, `bindings_snapshot()`/a future `InputStats` counter is the extension point — never a raw reference into `Impl`. |
| **P-7** Pool-owned RAII lifecycle | Partial — `touch.mempool` precedent | Legacy already pool-owns touch-button memory via `Mem_AllocPool("Touch")` (in_touch.c:94) — the rewrite continues that pattern through the memory subsystem's `create_<thing>`/`pool_new<T>` idiom rather than reinventing allocation. |
| **G-2** Game ABI v2 | Consumer, not owner | The `cldll_func_t`/`cl_enginefunc_t` slots this module drives (External ABI contracts) are exactly the kind of context-less, non-reentrant callback surface G-2 will eventually rework; input's job today is only to keep its **internal** surface context-first (P-3) so a v2 slot redesign has something sane to bind to later. No action owed now beyond that. |
| **G-3** Dedicated debug thread / **G-4** expanded debugging | Consumer via P-4 | Any future input-state overlay/debug-thread export reads `bindings_snapshot()` — same rule as P-2/P-4 rows, not a distinct door. |
| **P-1** Main-thread service inbox | None — reasoned | Input is entirely `T_Main` (event pump requires `SDL_INIT_VIDEO` on the same thread that drains it); there is no off-main mutation source today, so the MPSC inbox has no producer here. If G-1/G-5 ever inject synthetic events from another thread, they marshal through the inbox to reach `T_Main` and call the same `IEventSource`-shaped entry points — not a bespoke channel. |
| **P-5** Narrowest-state signatures | None beyond ordinary practice — reasoned | No aggregate-spanning free functions were surveyed in this material that would need the P-5 sub-aggregate treatment; the class-based `Input`/`IEventSource`/`IWindowControls` design already narrows by construction. |
| **P-6** Services are satellites | None — reasoned | Satellite-components analysis above found no candidate that clears the Q-11 bar; nothing here becomes a separate target. |
| **P-8** Annotation discipline | Applies generally, not input-specific | Standard QN annotation duty on every new type in the eventual implementation — no input-specific seam beyond the universal rule. |

## Threading

Ratified thread model (FACTBASE.md): **input = T_Main**, full stop — no
NetIO/AudioDecoder-style split exists or is planned for this subsystem.
Legacy is single-threaded throughout (SDL event pump requires
`SDL_INIT_VIDEO` and is drained synchronously from the main loop; no
background OS callback thread delivers `SDL_CONTROLLERAXISMOTION`/
`SENSORUPDATE` — SDL's own sensor polling, if any, still surfaces through
the single app-drained event queue, per R10.3 Uncertainties, unconfirmed
at the SDL-internals level but consistent with every other observation).

Hazard classes carried over from the fragments' Step-2 classification
(`analyse-threading`: Safe-RO / Safe-TLS / Race-static-buf /
Race-lazy-init / Race-shared / Signal-unsafe):

| State | Legacy thread | xash3dpp thread | Class | Note |
|---|---|---|---|---|
| `keys[265]` | T_Main only | T_Main only | Safe-TLS-equivalent | single-writer-single-thread; not literal TLS but exclusively T_Main so no sync needed (R10.1) |
| static `tinystr[16]` in `Key_KeynumToString` | T_Main | T_Main | **Race-static-buf (latent)** | unreachable today (single thread) but a P-4 door-rule violation risk for any future off-main introspection consumer — **must not survive the port as a shared buffer** (R10.1) |
| `key_rotate` cvar reads | T_Main | T_Main | Safe-RO today | would become Race-shared if a non-Main reader appeared before the §8.3 `shared_mutex` cvar retrofit lands (R10.1) |
| `Key_WriteBindings` (config save) | T_Main | T_Main | Safe-TLS-equivalent | file I/O routed through filesystem, sibling-owned (R10.1) |
| `joyaxis[]`/`joyaxesmap[]`, `gyrocal`, `joy_gyro_speed*`, `gyro_speed` | T_Main (SDL event pump) | T_Main | Safe-TLS-equivalent (working classification) | written by the SDL controller-event handler, read by `Joy_FinalizeMove`/`IN_GyroFinalizeMove`, both on the same drain thread; R10.3 flags this as *not independently confirmed* against SDL's internal sensor-thread model — accepted as the working classification per this draft, pending confirmation |
| `touch`/`osk` file-scope structs | T_Main | T_Main | Safe-TLS-equivalent | no locks, no atomics anywhere in either file; no Race-shared/Signal-unsafe pattern found (R10.4) |
| module statics in input.c (`in_mouseactive`, `in_mouseinitialized`, `in_lastvalidpos`, `in_mouse_savedpos`, `in_mstate`, `inputstate`) | T_Main | T_Main | Safe-TLS | single real thread acts as implicit TLS domain (R10.5) |
| `s_bRawInput`/`s_bMouseGrab` edge latches | T_Main | T_Main | **Race-static-buf (shape)**, Safe-TLS in practice | function-static mutable state; would race hard under any future multi-thread input dispatch — must become member state in the port (R10.5) |
| `moveflags` in `IN_JoyAppendMove` | T_Main | T_Main | **Race-static-buf (shape)**, Safe-TLS in practice | persistent frame-to-frame edge state; must become explicit owned state if collect/append ever split onto different call sites (R10.5) |
| `IN_LockInputDevices`'s cvar-flag mutation | T_Main | T_Main | Safe-RO today | plain `SetBits`/`ClearBits`, no atomics — Race-shared if any other thread reads/writes cvar flags concurrently (R10.5) |
| `clgame.client_dll_uses_sdl` | T_Main (set at load, read every frame) | T_Main | **Race-lazy-init (shape)**, benign today | would need synchronization if client-DLL (re)load ever moved off T_Main (R10.5, med confidence) |

**MockEventSource injection contractually T_Main**: per the ratified
mock-event-source decision (FACTBASE.md — "mock event source, no window"),
any synthetic-event injection path (testing, or a future G-5/G-1
scripting/MCP driver) must marshal onto `T_Main` before calling into
`IEventSource`'s consumers — there is no cross-thread contract to design
here, only the discipline that injection never bypasses `T_Main`.

**Assert/annotation duty**: no `assert_thread_role` call sites exist yet
(0 TUs). When the class-based `Input`/`IEventSource`/`IWindowControls`
surface is implemented, every mutator gets `@thread-safety: T_Main-only`
plus (once thread-role assertion is wired for this subsystem, matching the
networking precedent's eventual flip) `assert_thread_role(ThreadRole::Main)` —
there is no `T_Input`-split analog to networking's `T_NetIO` planned or
warranted; this is a permanent Main-thread confinement, not a staged door.

## Open questions

| ID | Question | Recommended shape | Blocks |
|---|---|---|---|
| **INP-OQ-1** | What mechanism carries "command context" (e.g. the keynum parameter appended to `+`/`-` button commands, in_keys.c:595-632) through to the rewrite's command dispatch — does `cmd_cvar`'s existing `ParamSpec`/context surface already cover a keynum-tagged button command, or does input need a small extension? | Depends on campaign **B5** (not yet landed at recon time) — defer resolution until B5's `cmd_cvar` shape is final; input's `Key_AddKeyCommands` equivalent should reuse whatever B5 settles on rather than inventing a parallel mechanism. | Blocks: finalizing the `bind`/`unbind` command-registration shape in the `Input` class design. |
| **INP-OQ-2** | Where does the touch/OSK **drawing** fence resolve — R10.4's Chunk-12 fence list (`ref.dllFuncs.GL_LoadTexture`/`FillRGBA`/`Color4ub`/`R_DrawStretchPic`, `Con_DrawString`, `CL_DrawCharacter`, plus the whole-function fences `Touch_Draw`/`Touch_DrawButtons`/`OSK_Draw`/etc.) needs an owner once Chunk 12 (client/renderer) is scoped — input owns the *data model* (button lists, layout, gesture state) but not the render calls. | Recommend: input's `Input` class exposes read-only iteration over button/layout state; Chunk 12 owns the actual draw calls behind `ref.dllFuncs`. No seam needs to be built in Chunk 10 beyond making the data model iterable/typed (which P-4 already requires). | Blocks: Chunk 12 boundary-spec authoring needs this data-model/draw-call split as an input. |
| **INP-OQ-3** | Clipboard (`Platform_GetClipboardText`/`SetClipboardText`) placement — R10.2 found no confirmed input-core consumer (only a `system.c:123` crash-dialog call), and its `SDL_INIT_VIDEO`-but-no-hWnd dependency shape doesn't cleanly fit either `IEventSource` or `IWindowControls`. | **Re-deferred to the window decision** (per assignment) — clipboard is a window-manager-scoped SDL facility; its final placement should follow whatever the eventual `IWindowControls`-vs-platform split decides for other no-hWnd-but-video-dependent functions, not be special-cased here. | Blocks: nothing in Chunk 10 itself (no confirmed input-core caller); revisit when the window/platform boundary is drawn for real. |

### Unresolved from the fragments (carried forward, not new OQs)

- `Key_EnableTextInput`'s OSK branch returns before the
  `host.textmode = enable` assignment — whether `OSK_EnableTextInput`
  independently sets `host.textmode` on its own path was not verified
  (R10.1 Uncertainties; out of in_keys.c's file scope).
- Whether any file enforces a bind-guard on `` ` ``/`~` (unlike the
  explicit `K_ESCAPE` unbind refusal) was not verified — a user could
  `bind ~ "cmd"` and it would be silently unreachable (R10.1
  Uncertainties).
- `Touch_ExportButtonToConfig`'s `keepAspect=true` path appears
  structurally dead within `in_touch.c`/`in_osk.c` (no caller found) —
  not confirmed dead engine-wide (R10.4 Uncertainties).
- SDL3 backend (`joy_sdl3.c`) behavioural parity with the SDL2 backend
  surveyed here was explicitly out of R10.3's scope.
