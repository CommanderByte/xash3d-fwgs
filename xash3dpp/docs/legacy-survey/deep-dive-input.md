# Deep Dive — Input (Chunk 10) — DRAFT

> Assembled 2026-07-19 by merging five A1 recon fragments, each an
> independent read of one slice of the legacy input subsystem. Method: each
> fragment agent read its assigned legacy files cold, cited every
> load-bearing claim as `file:line | claim | verbatim evidence | confidence
> | spec-section target`, and logged residual doubt in its own
> `Uncertainties` list. This document concatenates all five without
> re-deriving or adjudicating conflicts — conflicts and gaps are called out
> inline where fragments touch the same code from different angles (e.g.
> R10.1 vs R10.5 both cover `Key_Event`'s call sites). Nothing here has been
> cross-checked against the working tree by this assembly pass; citations
> are exactly as reported by each fragment.

> Fragments merged: **R10.1** (in_keys.c — keys/bindings/routing),
> **R10.2** (Platform_* input/window seam surface across platform.h +
> SDL2/evdev backends), **R10.3** (in_joy.c/in_gyro.c/joy_sdl2.c —
> joystick/gamepad/gyro), **R10.4** (in_touch.c/in_osk.c — touch + OSK
> event model, EVENT MODEL ONLY, rendering fenced to Chunk 12/13),
> **R10.5** (input.c frame drivers + full ABI slot list, usercmd/input ABI
> recon).

---

## Part 1 — Keys and bindings (source: R10.1, `engine/client/input/in_keys.c`)

### 1.1 Interface

| legacy file:line | claim | verbatim evidence (≤3 lines) | confidence |
|---|---|---|---|
| in_keys.c:168-173 | `Key_IsDown(keynum)` returns `false` for `keynum == -1`, else `keys[keynum].down`; no upper-bound check | `if( keynum == -1 )\n\t\treturn false;\n\treturn keys[keynum].down;` | high |
| in_keys.c:188-217 | `Key_StringToKeynum` maps a name string to a keynum: single-char strings return the char's ordinal, `"0x.."` parses as raw hex (bounds-checked), else linear scan of `keynames[]`, -1 on no match | `if( !str[1] )\n\t\treturn str[0];\n\n\t// check for hex code\n\tif( str[0] == '0' \&\& str[1] == 'x' )` | high |
| in_keys.c:227-254 | `Key_KeynumToString` inverse: out-of-range → `"<OUT OF RANGE>"`, -1 → `"<KEY NOT FOUND>"`, printable ASCII (excl. `"`,`;`,SCROLLLOCK) → itself via **static** buffer, else `keynames[]` scan, else hex fallback into the same static buffer | `static char\ttinystr[16];` ... `tinystr[0] = keynum;\n\t\ttinystr[1] = 0;\n\t\treturn tinystr;` | high |
| in_keys.c:261-274 | `Key_SetBinding` frees prior binding (`Mem_Free`) then `copystring()`s new one; keynum==-1 no-op | `if( keys[keynum].binding )\n\t{\n\t\tMem_Free((char *)keys[keynum].binding );` | high |
| in_keys.c:282-287 | `Key_GetBinding` returns `NULL` for -1, else raw binding pointer (no copy) | `if( keynum == -1 )\n\t\treturn NULL;\n\treturn keys[keynum].binding;` | high |
| in_keys.c:294-318 | `Key_GetKey` (static) does case-insensitive **prefix** compare of every bound key's binding against `pBinding`; first match wins, scan order = keynum order | `if( *p == '+' )\n\t\t\tp++;\n\n\t\tif( !Q_strnicmp( p, pBinding, len ) )\n\t\t\treturn i;` | high |
| in_keys.c:326-334 | `Key_LookupBinding(pBinding)` = `Key_KeynumToString(Key_GetKey(pBinding))`; `NULL` only when no key found | `if( key == -1 )\n\t\treturn NULL;\n\n\treturn Key_KeynumToString( key );` | high |
| in_keys.c:462-484 | `Key_WriteBindings(file_t*)` writer: `unbindall` first, then one `bind "<name>" "<escaped>"` per bound key, always double-quoted for mod-compat | `FS_Printf( f, "unbindall\n" );` ... `// CoF expects key to be enclosed in double quotes` | high |
| in_keys.c:492-503 | `Key_Bindlist_f` prints every non-empty binding raw, unescaped, no `unbindall` header — diverges from `Key_WriteBindings` | `Con_Printf( "%s \"%s\"\n", Key_KeynumToString( i ), keys[i].binding );` | high |
| in_keys.c:341-366 | `unbind`: argc!=2 error; invalid keyname error; `K_ESCAPE` refused ("Can't unbind ESCAPE key") — only permanently-protected key | `if( b == K_ESCAPE )\n\t{\n\t\tCon_Printf( "Can't unbind ESCAPE key\n" );\n\t\treturn;\n\t}` | high |
| in_keys.c:373-386 | `unbindall`: clears all, re-applies exactly two hardcoded defaults (`K_ESCAPE`, `K_START_BUTTON` → `cancelselect`) | `Key_SetBinding( K_ESCAPE, "cancelselect" );\n\tKey_SetBinding( K_START_BUTTON, "cancelselect" );` | high |
| in_keys.c:393-407 | `resetkeys`: clears all, replays **entire** `keynames[]` default table | `for( i = 0; i < ARRAYSIZE( keynames ); i++ )\n\t\tKey_SetBinding( keynames[i].keynum, keynames[i].binding );` | high |
| in_keys.c:414-453 | `bind`: argc<2 usage; argc==2 query; argc>2 re-joins args into fixed `char cmd[1024]` (no overflow guard beyond `Q_strncat`'s internal bound) | `cmd[0] = 0;\n\n\tfor( i = 2; i < c; i++ )\n\t{\n\t\tQ_strncat( cmd, Cmd_Argv( i ), sizeof( cmd ));` | high |
| in_keys.c:575-579 | `Key_Init`: bind/unbind/unbindall/resetkeys via `Cmd_AddRestrictedCommand` (trust-gated); bindlist/makehelp via plain `Cmd_AddCommand` | `Cmd_AddRestrictedCommand( "bind", Key_Bind_f, ...);` ... `Cmd_AddCommand( "bindlist", Key_Bindlist_f, ...);` | high |
| in_keys.c:512-556 | `Cmd_GetKeysList` (bind autocomplete): `*` wildcard or case-insensitive prefix match; stack-allocated `string keys_strings[ARRAYSIZE(keys)]` (265 entries) | `string keys_strings[ARRAYSIZE( keys )];` | high |
| in_keys.c:863-876 | `Key_EnableTextInput(enable,force)`: if `osk_enable.value`, delegates entirely to `OSK_EnableTextInput` and returns; else calls `Platform_EnableTextInput` only on state transition (or `force`), then sets `host.textmode = enable` | `if( osk_enable.value )\n\t{\n\t\tOSK_EnableTextInput( enable, force );\n\t\treturn;\n\t}\n\tif( enable \&\& ( !host.textmode \|\| force ))\n\t\tPlatform_EnableTextInput( true );` | high |
| in_keys.c:883-911 | `Key_SetKeyDest(key_dest)`: calls `IN_ToggleClientMouse(key_dest, cls.key_dest)` **before** the switch (old dest still live), per-branch `Key_EnableTextInput`, then assigns `cls.key_dest`; unknown dest → `Host_Error` | `IN_ToggleClientMouse( key_dest, cls.key_dest );\n\n\tswitch( key_dest )\n\t{\n\tcase key_game:` | high |
| input.c:174-196 | `IN_ToggleClientMouse(newstate,oldstate)`: no-op if equal; else cursor type + evdev grab toggle before mouse-visibility/`m_ignore` handling | `if( newstate == oldstate )\n\t\treturn;\n\n\tif( newstate == key_menu \|\| newstate == key_console )\n\t{\n\t\tPlatform_SetCursorType( dc_arrow );` | high |
| in_keys.c:918-940 | `Key_ClearStates`: **skipped when `cls.changelevel`**; else mouse-range keys get synthetic `IN_MouseEvent` release, others get synthetic `Key_Event` release, then fields force-zeroed; `clgame.dllFuncs.IN_ClearStates()` invoked if loaded | `if( cls.changelevel )\n\t\treturn;` ... `if( i >= K_MOUSE1 \&\& i <= K_MOUSE5 )\n\t\t\tIN_MouseEvent( i - K_MOUSE1, false );` | high |
| in_keys.c:949-969 | `CL_CharEvent(key)`: console glyphs `` ` ``/`~` always dropped; console-open-not-visible also drops `` ` ``/`?`; routes to `Con_CharEvent` or `UI_CharEvent` | `` if( key == '`' \|\| key == '~' ) return; `` | high |
| in_keys.c:978-990 | `Key_ToUpper` fallback shift table: `-`→`_`, `=`→`+`, `;`→`:`, `'`→`"`, else `Q_toupper` | `if( keynum == '-' )\n\t\treturn '_';` | high |

### 1.2 Owned state

| legacy file:line | claim | confidence |
|---|---|---|
| in_keys.c:22-28,43 | `enginekey_t` (binding ptr, `down:1`, `gamedown:1`, `repeats:30`) in file-scope static array `keys[265]` | high |
| in_keys.c:37-43 | 265 = over-provisioned vs ~255 real keys + 9 international slots | high |
| in_keys.c:45-159 | `keynames[]` static const table, 101 rows (name, keynum, default-bind) — count corrected 2026-07-20 by the campaign close-out audit | high |
| in_keys.c:229 | Static `tinystr[16]` return buffer in `Key_KeynumToString`, reused across calls | high |
| in_keys.c:161 | `key_rotate` cvar (`FCVAR_ARCHIVE\|FCVAR_FILTERABLE`) | high |
| in_keys.c:560 (client.h) | `cls.key_dest` — owned by broader `client_t cls`, not local | high |

### 1.3 Quirks and invariants — `Key_Event`'s 13-step order (verbatim)

| legacy file:line | claim | confidence |
|---|---|---|
| in_keys.c:709-855 | Full routing order: (1) `Key_Rotate`; (2) OSK first refusal; (3) stale key-up guard; (4) cinematic filter (compile-gated); (5) client-DLL first refusal (`down\|\|gamedown`); (6) autorepeat accounting/suppression; (7) unbound-key console warning (keynum≥200, down only); (8) unconditional `VGui_KeyEvent`; (9) console-key hardcode; (10) ESC special-case (`key_game` only); (11) menu-dest char synthesis + `UI_KeyEvent`; (12) key-up-only short-circuit; (13) final key-down dispatch by `key_dest` | high |
| in_keys.c:713-716 | Step 2 evidence: OSK checked before `keys[key].down` write | high |
| in_keys.c:718-723 | `kb` (binding snapshot) read happens one line before `keys[key].down` assignment — unaffected by this event's down-state | high |
| in_keys.c:734-750 | Step 5: gated `key_dest==key_game && (down\|\|gamedown)` — a key-up still routes if DLL claimed the matching down | high |
| in_keys.c:736-749 | `pfnKey_Event` returning **0** (falsy) = "handled" (inverted sense); engine sets `gamedown=true` only on first (`repeats==0`) down, clears on up, returns without further routing | high |
| in_keys.c:641-658 | `Key_IsAllowedAutoRepeat`: always true outside `key_game`; inside `key_game` only Backspace/Pause/PgUp/PgDn (+KP) autorepeat | high |
| in_keys.c:753-765 | Autorepeat suppression drops the entire back half of `Key_Event` for repeats>1 on non-allowlisted keys | high |
| in_keys.c:774-783 | Console-key hardcode: `` ` ``/`~` never reaches `Key_AddKeyCommands`; suppressed on down while `key_message`; up always no-op | high |
| in_keys.c:786-806 | ESC special-case only inside `key_game`; texture-atlas close OR mouse-visible-not-cinematic re-delegation, else falls through | high |
| in_keys.c:808-830 | Menu-dest char synthesis: only when `!gameui.use_extended_api && !host.textmode`; range `32..'z'`; hardcoded shift-case bump `+='A'-'a'` | high |
| in_keys.c:836-846 | Key-up short-circuit: **any** `down==false` only runs `Key_AddKeyCommands`, regardless of `key_dest` — intentional, for a `+`-action started before a mode switch | high |
| in_keys.c:595-632 | `Key_AddKeyCommands`: `+`-tokens get keynum appended (`"%s %i\n"`/`"-%s %i\n"`); non-`+` tokens fire down-only | high |
| in_keys.c:660-699 | `Key_Rotate`: only the 4 arrow keys, gated by exact float `==` on `key_rotate.value` (1/2/3); KP arrows not rotated | high |
| in_keys.c:237-238 | `Key_KeynumToString` ASCII fast path excludes `"`, `;`, SCROLLLOCK despite being in 33-126 range | high |
| in_keys.c:188-196,199-207 | Asymmetric bounds-check: single-char path has none, hex-code path does | med |
| host_sdl2.c:225-233 | UTF-8 fold lives outside in_keys.c: `SDL_TEXTINPUT` per-codepoint, force-folded via `Con_UtfProcessCharForce` when `!cls.accept_utf8`, before `CL_CharEvent` | high |
| in_osk.c:94-99 | `OSK_KeyEvent` no-op (`return false`) unless `osk.enable && osk_enable.value` — dead weight on desktop by default | high |
| in_keys.c:918-940 | `Key_ClearStates` changelevel-skip: held keys keep state across level transition | high |
| in_keys.c:883-886 | `Key_SetKeyDest`/`IN_ToggleClientMouse` ordering: called with new-dest, old-dest (pre-mutation) — reentry hazard if any callee re-enters `Key_SetKeyDest` | med |

### 1.4 Threading (R10.1)

| legacy file:line | claim | legacy thread | xash3dpp thread | class |
|---|---|---|---|---|
| in_keys.c:43 `keys[265]` | file-scope static, written by Key_Event/ClearStates/SetBinding | T_Main only | T_Main only | Safe-TLS-equivalent |
| in_keys.c:229 `tinystr[16]` | static return buffer | T_Main | T_Main (P-4 door risk if off-main reader ever wants a key-name string) | Race-static-buf (latent) |
| in_keys.c:161 `key_rotate` | inline cvar read in `Key_Rotate` | T_Main | T_Main; would become Race-shared pre-§8.3-retrofit if a non-Main reader appeared | Safe-RO today |
| in_keys.c:462-484 `Key_WriteBindings` | config save, main-loop-driven | T_Main | T_Main | Safe-TLS-equivalent |

### 1.5 R10.1 Uncertainties

- `Key_EnableTextInput`'s OSK branch returns before `host.textmode = enable` at line 875 — whether `OSK_EnableTextInput` independently sets `host.textmode` on its own path not verified (out of file scope).
- `` ` ``/`~` unbindable via `Key_AddKeyCommands`, but no verified guard blocks *binding* to them via `bind` (unlike explicit `K_ESCAPE` unbind refusal) — a user could `bind ~ "cmd"` silently unreachable.
- Full `keydest_t` enum enumeration not independently confirmed beyond this file's switch usages.
- `Key_AddKeyCommands`'s `button[1024]`/`cmd[1024]` overflow hazard against long bindings not verified — flagged, not asserted as a bug.

---

## Part 2 — Platform_* input/window seam surface (source: R10.2)

Scope: every `Platform_*` function in `engine/platform/platform.h:245-381`
that is input- or window-related, its SDL2 impl, consumer sites across
`engine/client/input/*`, `vgui_draw.c`, `cl_game.c`/`cl_gameui.c`/
`cl_mobile.c`, `cl_main.c`/`cl_view.c`, `system.c`/`host.c`/`host_state.c`,
the `host_parm_t` fields touched, and the evdev seam
(`platform.h:375-381`). Non-`Platform_`-prefixed window-management
functions in the same header block are out-of-scope-entirely
(renderer/refresh boundary).

### 2.1 Interface — every Platform_* input/window function

| legacy file:line | claim | confidence |
|---|---|---|
| platform.h:246-247 | `Platform_Vibrate`/`Vibrate2` — SDL2-gated, no-op stub otherwise | high |
| joy_sdl2.c:334-352 | `Platform_Vibrate2` uses `SDL_GameControllerRumble` on `g_current_gamepad`; no window touch | high |
| cl_mobile.c:35 | Sole consumer: mobile haptics DLL export | high |
| platform.h:261 | `Platform_PreCreateMove` — SDL/DOS-gated | high |
| host_sdl2.c:446-453 | Impl: `SDL_GetRelativeMouseState`/`SDL_ShowCursor`, gated `m_ignore.value` | high |
| cl_main.c:699 | Sole consumer: once per client frame | high |
| platform.h:262 | `Platform_GetMousePos(int*,int*)` — GAME_EXPORT, SDL-gated | high |
| in_sdl2.c:44-53 | Impl: `SDL_GetMouseState` scaled by `refState.scale_x/y` | high |
| in_touch.c:2243, vgui_draw.c:264, cl_game.c:2907, input.c:146,353 | Five consumer sites | high |
| platform.h:263 | `Platform_SetMousePos(int,int)` — GAME_EXPORT, SDL-gated | high |
| in_sdl2.c:61-64 | Impl: `SDL_WarpMouseInWindow(host.hWnd,...)` — needs hWnd | high |
| input.c:162 | Consumer: restores last valid mouse position | high |
| platform.h:264 | `Platform_GetMouseGrab(void)` — SDL-gated | high |
| in_sdl2.c:240-243 | Impl: `SDL_GetWindowGrab(host.hWnd)` — needs hWnd | high |
| system.c:94-106 | Consumer: crash-path grab save/restore | high |
| platform.h:265 | `Platform_SetMouseGrab(qboolean)` — SDL-gated | high |
| in_sdl2.c:250-253 | Impl: `SDL_SetWindowGrab(host.hWnd,enable)` — needs hWnd | high |
| input.c:255,263; system.c:95,106 | Consumers: `IN_SetMouseGrab`, crash dialog | high |
| platform.h:266 | `Platform_SetCursorType(VGUI_DefaultCursor)` — SDL-gated | high |
| in_sdl2.c:185-233 | Impl: sets `host.mouse_visible`, `VGui_UpdateInternalCursorState`, `SDL_SetCursor`/`ShowCursor`, warps via `Platform_SetMousePos` using `window_center_x/y` | high |
| in_touch.c:564,569; vgui_draw.c:272; cl_gameui.c:1147; input.c:183,191; system.c:396 | Six consumer sites | high |
| platform.h:267 | `Platform_GetClipboardText(char*,size_t)` — SDL-gated | high |
| in_sdl2.c:86-103 | Impl: `SDL_GetClipboardText`/`SDL_free`; no hWnd | high |
| system.c:123 | Sole consumer: crash-note copy path | high |
| platform.h:268 | `Platform_SetClipboardText(const char*)` — SDL-gated | high |
| in_sdl2.c:111-114 | Impl: `SDL_SetClipboardText` | high |
| — | No call sites found anywhere under `engine/` for `SetClipboardText` | med |
| platform.h:285 | `Platform_RunEvents(void)` — SDL/DOS-gated, top-level event pump | high |
| host_sdl2.c:426-436 | Impl: `while(...) SDL_PollEvent → SDLash_EventHandler`, gated `host.status != HOST_CRASHED`, needs `SDL_INIT_VIDEO` | high |
| host_state.c:155 | Sole consumer: once per host frame | high |
| platform.h:286 | `Platform_MouseMove(float*,float*)` — SDL/DOS-gated | high |
| in_sdl2.c:72-78 | Impl: `SDL_GetRelativeMouseState`; relative delta, no hWnd param | high |
| input.c:557 | Sole consumer: `IN_Move`/mouse-look sampling | high |
| platform.h:297 | `Platform_EnableTextInput(qboolean)` — SDL2+/PSVita/DOS/evdev-gated | high |
| in_sdl2.c:124-127 | Impl: `SDL_StartTextInput()`/`SDL_StopTextInput()` | high |
| in_keys.c:870-875 | Consumer: `Key_EnableTextInput`, gates/writes `host.textmode` | high |
| platform.h:303-304 | `Platform_JoyInit`/`Shutdown` — SDL2-gated | high |
| joy_sdl2.c:371-419 | Impl: `SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)`; no `SDL_INIT_VIDEO` need | high |
| in_joy.c:620,634 | Consumers: `IN_JoyInit`/`IN_JoyShutdown` | high |
| platform.h:305 | `Platform_CalibrateGamepadGyro(void)` — SDL2-gated | high |
| joy_sdl2.c:329-332 | Impl delegates to `SDLash_RestartCalibration()` | high |
| in_joy.c:563 | Sole consumer | high |
| platform.h:306 | `Platform_GetKeyModifiers(void)` — SDL2-gated | high |
| in_sdl2.c:261-290 | Impl: `SDL_GetModState()` → `key_modifier_t` bitmask; no hWnd | high |
| — | No call sites found under engine/client or engine/common | med |
| platform.h:314-319 | `Platform_SetTimer(float)` — inline, Linux-only body, no-op elsewhere | high |
| cl_main.c:3642, cl_view.c:580 | Consumers: `cl_maxframetime` watchdog, view reset | high |
| platform.h:321-330 | `Platform_Input(void)` — inline dispatcher to `Wcon_Input()`/`Posix_Input()`; **dedicated-server stdin console-line input**, not SDL game input | high |
| host.c:489 | Sole consumer: console command-line pump in `Host_Frame` | high |
| platform.h:368 | `Platform_Minimize_f(void)` — unconditional window-manager verb | high |
| vid_sdl2.c:46-50 | Impl: `if(host.hWnd) SDL_MinimizeWindow(host.hWnd)` — needs hWnd | high |
| — | No direct call site found; resolved via `minimize` console command (cmd_cvar's surface) | low |

### 2.2 Window-management block — NOT Platform_-prefixed (context only, out of scope)

| legacy file:line | claim | confidence |
|---|---|---|
| platform.h:354-370 | `R_Init_Video`, `R_Free_Video`, `VID_SetMode`, `R_ChangeDisplaySettings`, `R_MaxVideoModes`, `R_GetVideoMode`, `GL_GetProcAddress`, `GL_UpdateSwapInterval`, `GL_SetAttribute/GetAttribute`, `GL_SwapBuffers`, `SW_LockBuffer/UnlockBuffer/CreateBuffer` — refresh/renderer-owned | high |
| platform.h:369, vid_sdl2.c:1086-1153 | `R_GetWindowHandle` returns native window handle for renderer's GL/Vulkan context — renderer↔platform contract, not input | high |
| platform.h:370, vid_sdl2.c:1171-1188 | `VID_Info_f` — console command, SDL window/video-driver diagnostics — renderer diagnostics | high |

### 2.3 host_parm_t fields input touches

| legacy file:line | claim | confidence |
|---|---|---|
| common.h:301 | `hWnd` — "main window"; read by SetMousePos/GetMouseGrab/SetMouseGrab/Minimize_f/R_GetWindowHandle/VID_Info_f | high |
| common.h:330 | `mouse_visible:1` — "vgui override cursor control (never change outside Platform_SetCursorType!)"; also read in_keys.c:797, input.c:284,289 | high |
| common.h:334, in_keys.c:819,870-875 | `textmode:1` — gates raw ASCII routing to text entry vs bind lookup; written only via `Key_EnableTextInput` | high |
| common.h:340-341, in_sdl2.c:227 | `window_center_x/y` — "for IN_MouseMove() easy access"; consumed by `Platform_SetCursorType`'s warp-to-center | high |
| common.h:289, host_sdl2.c:430, host.c:489 | `status` (host_status_t) — read by RunEvents pump-exit and console pump; input core doesn't write it | high |

### 2.4 The evdev seam (platform.h:375-381)

| legacy file:line | claim | confidence |
|---|---|---|
| platform.h:375-381 | Five-function evdev surface, `XASH_USE_EVDEV`-gated only | high |
| in_evdev.c:334,402,423,445,455 | All five implemented in `engine/platform/linux/in_evdev.c`; not part of SDL2 backend | high |
| input.c:186,194,425,456,562,620 | Six call sites, all `XASH_USE_EVDEV`-gated, run **alongside** SDL calls (not a replacement) | high |

### 2.5 A leak: input core calls raw SDL directly, bypassing Platform_*

| legacy file:line | claim | confidence |
|---|---|---|
| input.c:212-246 | `IN_SetRelativeMouseMode` calls `SDL_GetRelativeMouseState`/`SDL_SetRelativeMouseMode`/`SDL_SetWindowRelativeMouseMode(host.hWnd,...)` directly — NOT routed through any `Platform_*` seam; direct SDL2/SDL3 dep inside "input core" C code | high |

### 2.6 External ABI contracts (R10.2)

| legacy file:line | claim | confidence |
|---|---|---|
| platform.h:262-268 | Only `Platform_GetMousePos`/`Platform_SetMousePos` carry `GAME_EXPORT` — exported across engine/game-DLL ABI | high |
| in_sdl2.c:44-53 | `GetMousePos`'s `GAME_EXPORT`-visible behaviour bakes in `refState.scale_x/y` — must be preserved for VGUI/`cl_game.c:2907` | high |
| platform.h:339-346 | `rserr_t`/window-mode/ref-window-type enums shared with RenderAPI — renderer/refresh boundary, not input's | high |
| platform.h:339-346 | No other input-facing struct/enum in 245-381 crosses the game-DLL ABI | med |

### 2.7 Two-column partition: IEventSource vs IWindowControls vs out-of-scope

Rationale keys: **needs hWnd** = calls an SDL window-handle API; **needs
SDL_INIT_VIDEO** = requires video subsystem up without a direct hWnd param;
**refState scaling** = consumes renderer-owned DPI/logical scale.

**IEventSource** (pure input, no window-property mutation): `Platform_RunEvents`,
`Platform_MouseMove`, `Platform_GetMousePos` (GAME_EXPORT, refState-scaled),
`Platform_PreCreateMove`, `Platform_GetKeyModifiers`, `Platform_JoyInit`/
`Shutdown`, `Platform_CalibrateGamepadGyro`, `Platform_Vibrate`/`Vibrate2`,
`Evdev_Init`/`Shutdown`/`SetGrab`/`IN_EvdevMove`/`IN_EvdevFrame`,
`Platform_EnableTextInput`, `Platform_SetTimer` (by elimination),
`Platform_Input` (ambiguous — see Uncertainties).

**IWindowControls** (window properties, null-backed XASH3DPP-STUB(chunk12)):
`Platform_SetMousePos` (needs hWnd), `Platform_GetMouseGrab`/`SetMouseGrab`
(needs hWnd), `Platform_SetCursorType` (transitively needs hWnd + window_center
fields), `Platform_Minimize_f` (needs hWnd), `Platform_GetClipboardText`/
`SetClipboardText` (no hWnd param, needs SDL_INIT_VIDEO — placement soft, no
confirmed consumer either).

**Out-of-scope-entirely**: `R_Init_Video`/`R_Free_Video`/`VID_SetMode`/
`R_ChangeDisplaySettings`/`R_MaxVideoModes`/`R_GetVideoMode`/`GL_*`/`SW_*`
(renderer), `R_GetWindowHandle` (renderer↔platform), `VID_Info_f` (renderer
diagnostic), `Platform_Input` (if routed to console/system seam instead —
orchestrator judgment call).

### 2.8 R10.2 Uncertainties

- `Platform_GetKeyModifiers` and the clipboard pair had no confirmed input-core call site in this survey's grep pass — treat as med/low confidence "no consumer," not settled absence.
- `Platform_Input` is genuinely ambiguous between `IEventSource` and out-of-scope: event/poll-shaped but payload is dedicated-server console text, sole consumer in `host.c:489` (host frame pump, not input core). Recommend routing to whatever seam owns dedicated-server console/stdin.
- `Platform_SetTimer` classed under `IEventSource` only by elimination — orchestrator may prefer ruling it out-of-scope entirely as a `platform` (time/sleep) surface item.
- Clipboard's window/video dependency verdict is soft — inferred from SDL2 API shape, not xash3dpp-side evidence.
- Vibrate/evdev placed in `IEventSource` by "no window-property character" reasoning, not positive evidence — a third bucket (`IHapticOutput`) may be preferable.

---

## Part 3 — Joystick/gamepad + gyro (source: R10.3, `in_joy.c`, `in_gyro.c`, `joy_sdl2.c`)

Sibling-scope respected: gamepad/gyro input is not currently part of
`platform`'s owned surface (time/sleep/dynlib/paths/console/crash/sockets).

### 3.1 Interface

| legacy file:line | claim | confidence |
|---|---|---|
| input.h:127-136 | Engine-side logical axis enum, 6 axes, `MAX_AXES` sentinel | high |
| input.h:138-144 | 4-state gyro calibration enum: `JOY_NOT_CALIBRATED/CALIBRATING/FAILED_TO_CALIBRATE/CALIBRATED` | high |
| input.h:114-125 | D-pad hat bitmask enum, combinable via OR | high |
| input.h:146-154 | Public joy API: `Joy_IsActive`, `Joy_SetCapabilities`, `Joy_SetCalibrationState`, `Joy_AxisMotionEvent`, `Joy_GyroEvent`, `Joy_FinalizeMove`, `Joy_DrawDebug`, `Joy_Init`, `Joy_Shutdown` | high |
| in_joy.c:84-97 | `Joy_IsActive` reads `joy_enable.value`; `Joy_SetCapabilities` writes `joy_have_gyro` via `Cvar_FullSet` | high |
| in_joy.c:104-110 | `Joy_SetCalibrationState` idempotent no-op if unchanged; writes READ_ONLY `joy_calibrated` via `Cvar_FullSet` (bypasses read-only guard) | high |
| in_joy.c:273-290 | `Joy_AxisMotionEvent`: bounds-check, remap via `joyaxesmap[]`, bounds-check again, drop if unchanged, dispatch trigger vs stick by `engineAxis >= JOY_AXIS_RT` | high |
| in_joy.c:299-303 | `Joy_GyroEvent(data)`: stores into both live-use buffer and a display buffer that survives per-frame clear | high |
| in_joy.c:312-363 | `Joy_FinalizeMove`: early-out if inactive; lazy re-parse of `joy_axis_binding` on `FCVAR_CHANGED`; applies stick axes; conditionally applies gamepad-gyro; always clears `joy_gyro_speed` | high |
| input.c:566-567 | Call order: `IN_GyroFinalizeMove` (device gyro) runs BEFORE `Joy_FinalizeMove` (gamepad, incl. gamepad-gyro), both additively accumulate into fw/side/pitch/yaw, followed by `Touch_GetMove` | high |
| in_gyro.c:38-48,77-120 | Device gyro: `IN_GyroInit` registers cvars; `IN_GyroCheckAvailability` (SDL-gated) one-shot-latches `gyro_available`; `IN_GyroEvent` stores rate; `IN_GyroFinalizeMove` applies orientation-aware rotation | high |
| platform.h:303-310 | Platform contract for joystick lifecycle + calibration; SDL-less builds no-op | high |
| platform.h:246-250 | `Platform_Vibrate`/`Vibrate2` contract; SDL-less no-op | high |
| joy_sdl2.c:296-327 | `SDLash_HandleGameControllerEvent` is the single SDL-event-loop entry: axis→`Joy_AxisMotionEvent`, button→`Key_Event`, device add/remove→list maintenance, sensor update→gyro pipeline | high |
| joy_sdl2.c:371-398,406-426 | `Platform_JoyInit` sets HIDAPI hints, inits `SDL_INIT_GAMECONTROLLER`, loads two mapping DB files, returns count of connected `SDL_IsGameController` devices; `Shutdown` closes/deactivates all | high |
| joy_sdl2.c:329-332 | `Platform_CalibrateGamepadGyro` trampolines to `SDLash_RestartCalibration` (also the `joy_calibrate_gyro` command handler) | high |

### 3.2 Quirks and invariants

| legacy file:line | claim | confidence |
|---|---|---|
| in_joy.c:60-61,317-337 | `joy_axis_binding` grammar: `s/f/y/p/r/l` → SIDE/FWD/YAW/PITCH/LT(r)/RT(l); default `"sfpyrl"`; unrecognized char → `MAX_AXES` (disabled) | high |
| in_joy.c:60-61 | Default binding letter order note; doc-string vs switch statement discrepancy (see 3.4 Uncertainties) | med |
| in_joy.c:317-337 | **Lazy re-parse quirk**: `joy_axis_binding` re-parsed only inside `Joy_FinalizeMove`, gated `FCVAR_CHANGED`, once per frame — never at init or via callback; first frame uses static-init default | high |
| in_joy.c:312-314 | `Joy_FinalizeMove` early-return skips BOTH stick axes AND gamepad-gyro if `joy_enable` false — same master switch | high |
| in_joy.c:154-187 | Trigger threshold math: edge-detected via prev/cur straddling threshold (not level check); default both thresholds 16384 | high |
| in_joy.c:229-264 | Stick deadzone: `.rawval` saved before zeroing; symmetric deadzone, default 4096 (`DEFAULT_JOY_DEADZONE`) | high |
| in_joy.c:254-263 | **UI-mode side effect**: SIDE/FWD axis updates in `key_menu`/`key_console` also synthesize D-pad hat motion (arrow keys) — gameplay mode never does this | high |
| in_joy.c:189-222 | `Joy_GetHatValueForAxis` uses separate higher threshold (`joy_side/forward_key_threshold`, default 24576, ~75% SHRT_MAX); only SIDE/FWD defined, `ASSERT(false)` otherwise | high |
| in_joy.c:339-342 | Sign/scale convention: fw MINUS, side PLUS; pitch/yaw scaled by `host.realframetime` (rate), fw/side treated as direct value | high |
| in_joy.c:344-360 | Gamepad-gyro gated by THREE ANDed conditions (`joy_gyro_enable`, `joy_have_gyro`, `joy_calibrated==JOY_CALIBRATED`); rad/s→deg/s; default 0.5 deg/s deadzone; yaw accumulates BOTH gyro-yaw AND gyro-roll (roll folds onto yaw) | high |
| in_joy.c:362 | `joy_gyro_speed` unconditionally cleared every `Joy_FinalizeMove` call regardless of branch taken; `joy_gyro_speed_display` NOT cleared here | high |
| in_gyro.c:89-119 | Device-gyro mirrors gamepad-gyro math but ALSO applies display-orientation sign flip + X/Y axis swap; gated `gyro_enable && gyro_available` only — no calibration-state gate | high |
| in_gyro.c:57-67 | `IN_GyroCheckAvailability` one-way latch; only SDL builds probe `SDLash_GyroIsAvailable()` | high |
| joy_sdl2.c:67-138 | **Gyro calibration state machine** (`gyrocal`): `RestartCalibration` resets, arms 5.0s window (`CALIBRATION_TIME`); `AccumulateCalibrationData` sums samples, filters continuous-run samples by `\|data-calibrated\|<=0.1`; `FinalizeCalibration` at 5s expiry needs `samples > min_samples` (>50% expected) else `FAILED_TO_CALIBRATE` (first-run only — continuous re-cal failure silently retried); success averages samples, sets `CALIBRATED`, `continuous=true`, re-arms — **calibration runs forever in background once first successful** | high |
| joy_sdl2.c:117-131 | Continuous-calibration filter: fixed hardcoded 0.1 threshold, not cvar-configurable | high |
| joy_sdl2.c:269-293 | `SensorUpdate` filters to active gamepad + `SDL_SENSOR_GYRO` only; suppresses `Joy_GyroEvent` delivery entirely during NON-continuous (first-run) calibration window; once continuous, calibrated data delivered every sample even during background re-cal | high |
| joy_sdl2.c:157-194 | `SetActiveGameController`: dedup no-op; disables gyro on OUTGOING controller first; on switch, unconditionally calls `RestartCalibration()` — **switching active gamepad always resets calibration**, even same device | high |
| joy_sdl2.c:196-234 | `GameControllerAdded`: Android "qwerty2" fake controller silently closed; first-connected-wins auto-activation, no priority cvar | high |
| joy_sdl2.c:39-48 | SDL axis→engine axis table (`g_axis_mapping[]`) independent of `joy_axis_binding` cvar — hand-tuned to agree with `"sfpyrl"` default, not shared code | high |
| joy_sdl2.c:334-352 | `Platform_Vibrate2`: negative val1/val2 → randomized `[0x7FFF,0xFFFF]` via `COM_RandomLong`, not "off"; `Platform_Vibrate` = compat shim always randomizing both motors | high |
| joy_sdl2.c:371-372,396 | `Platform_JoyInit`'s int return = count of enumerable devices at init, NOT success bool, NOT opened count (opening is async via `SDL_CONTROLLERDEVICEADDED`); `in_joy.c:614-622` discards the return value | high |
| in_joy.c:614-618 | `-noenginejoy` force-sets `joy_enable=0` READ_ONLY, returns before `Platform_JoyInit`; renamed from `-nojoy` to avoid colliding with game DLL's own joystick flag | high |
| in_joy.c:63-64,106-110 | `joy_have_gyro`/`joy_calibrated` are `FCVAR_READ_ONLY` yet written at runtime via `Cvar_FullSet` (bypasses guard) — "engine-writable, user-read-only" pattern, also used for `-noenginejoy`'s `joy_enable` override and `gyro_available` | high |

### 3.3 Cvar census, SDL maps, vibrate paths

| legacy file:line | claim | confidence |
|---|---|---|
| in_joy.c:48-72 | Full gamepad cvar table (24 cvars): pitch/yaw/side/forward sensitivities, lt/rt thresholds, side/forward key thresholds, 4 deadzones, `joy_axis_binding`, `joy_enable`, `joy_have_gyro`/`joy_calibrated` (READ_ONLY), gyro pitch/yaw/roll sensitivities+deadzones, `joy_gyro_enable`, `joy_debug` | high |
| in_gyro.c:20-27 | Device-gyro cvar table (8 cvars): `gyro_enable` (OFF by default, unlike `joy_gyro_enable`=ON), `gyro_available` (READ_ONLY), pitch/yaw/roll sensitivities+deadzones | high |
| defaults.h:169 | `DEFAULT_JOY_DEADZONE` = `"4096"` | high |
| joy_sdl2.c:23-37 | `g_button_mapping[]`: 21 entries; NSWITCH build swaps A/B and X/Y at table level to correct SDL's inverted layout | high |
| joy_sdl2.c:296-306,312 | Both axis/button handlers bounds-check SDL index before dereference — out-of-range silently dropped | high |
| joy_sdl2.c:302-303,308-311 | Every axis/button event calls `SetActiveGameController` first — any input from any controller steals "active" status | high |
| joy_sdl2.c:140-155 | `AddMappings` loads community DB text via `FS_LoadFile` (filesystem-owned), silently no-ops if not found; two files loaded unconditionally, later can override former | high |

### 3.4 R10.3 Uncertainties

- `joy_axis_binding` doc-string ("r - left trigger, l - right trigger") textually contradicts the switch statement (`'r'→JOY_AXIS_RT`, `'l'→JOY_AXIS_LT`) when read against `JOY_AXIS_RT`/`LT`'s own naming comments ("right trigger"/"left trigger"). Looks like a genuine legacy doc-string bug, flagged not asserted.
- Could not trace where `SDL_CONTROLLERSENSORUPDATE`'s `.type` field is filtered upstream of `SDLash_HandleGameControllerEvent`'s switch.
- SDL3 backend (`joy_sdl3.c`) not read — out of scope per assignment.
- Threading classification not formally attempted in this fragment (see boundary spec's Threading section for the adjudicated working classification: Safe-TLS-equivalent, single-thread-owned).
- `Joy_DrawDebug` (joy_debug cvar) draws via `ref.dllFuncs` — consistent with A0's noted render-satellite dependency; not enumerated in full detail here (Chunk 12/13 fence).

---

## Part 4 — Touch + OSK event model (source: R10.4, `in_touch.c`, `in_osk.c`)

Scope: EVENT MODEL ONLY. Rendering (`ref.dllFuncs.*`, `CL_DrawString`,
`CL_DrawCharacter`, `Con_DrawString`) is fenced to Chunk 12/13 — see §4.6.

### 4.1 Interface

| legacy file:line | claim | confidence |
|---|---|---|
| in_touch.c:2080 | `IN_TouchEvent` is the single external entry point for a raw touch sample | high |
| in_touch.c:61-66 (input.h) | Three event types: down/up/motion, no dedicated "cancel" | high |
| in_touch.c:2200 | `Touch_GetMove` per-frame output pull: accumulates into caller floats, self-clears yaw/pitch only | high |
| input.c:568 | `Touch_GetMove` called once per input frame from `IN_EngineAppendMove`, additive with joystick/gyro | high |
| in_touch.c:547,607,678,904,928 | Client-facing surface: `Touch_SetClientOnly`, `Touch_RemoveButton`, `Touch_HideButtons`, `Touch_AddClientButton`, `Touch_AddDefaultButton`/`ResetDefaultButtons` | high |
| in_touch.c:2209 | `Touch_KeyEvent` adapts mouse-emulation (K_MOUSE1) into synthetic `IN_TouchEvent` calls via static-tracked single "finger" | high |
| in_touch.c:2256 | `Touch_WantVisibleCursor`: touch+emulate cvars OR clientonly OR in-menu | high |
| in_osk.c:94 | `OSK_KeyEvent(key,down)` returns bool "consumed" — true means caller must swallow the raw key | high |
| in_keys.c:709-716 | `Key_Event` calls `OSK_KeyEvent` FIRST — full pre-emptive intercept | high |
| in_keys.c:863-869 | `Key_EnableTextInput` branches to `OSK_EnableTextInput` instead of platform IME when `osk_enable.value` | high |
| in_osk.c:168 | OSK's forward path for char entry is `CL_CharEvent(ch)`, gated `cls.accept_utf8` | high |
| in_osk.c:124,140,143 | OSK re-injects ENTER/BACKSPACE/TAB through `Key_Event`, sets `osk.sending` guard against self-interception | high |
| cdll_exp.h:80 | `pfnTouchEvent` game-DLL hook signature | high |

### 4.2 Owned state

| legacy file:line | claim | confidence |
|---|---|---|
| in_touch.c:90-137 | Single file-scope `touch` struct: two button lists (user/edit), gesture finger trackers, accumulated move output, edit cursor state, stroke/color config, aspect-ratio cache, texture handles | high |
| in_touch.c:48-70 | `touch_button_t` intrusive doubly-linked list, O(n) scans | high |
| in_touch.c:72-88 | Two list types: `list_user` (persisted) vs `list_edit` (fixed editor chrome), built by `Touch_InitEditor` | high |
| in_touch.c:142-143 | Separate module-static `g_DefaultButtons[]`/`g_DefaultButtonsLength` (realloc'd array, game-DLL templates), distinct from `list_user`/`list_edit` | high |
| in_touch.c:94 | Button memory pool-owned (`touch.mempool` via `Mem_AllocPool("Touch")`, torn down whole at shutdown) | high |
| in_osk.c:80-92 | Single file-scope `osk` struct: enable, layout index, shift, `sending` guard, cursor position into fixed 13x4 layout | high |
| in_osk.c:64-78 | Fixed compile-time `osk_keylayout[2][4]` — 2 case layouts x 4 rows, high-byte codes for TAB/SHIFT/BACKSPACE/ENTER | high |
| in_touch.c:822-854 | `Touch_AddButton` is the single constructor for both lists: replaces same-named button, `finger=-1` sentinel, appends at tail | high |

### 4.3 Quirks and invariants

| legacy file:line | claim | confidence |
|---|---|---|
| in_touch.c:24-32 | `touchButtonType` (6 variants) derived from command string prefix match (`_look`/`_move`/`_joy`/`_dpad`/`_wheel `/`_hwheel `), default `touch_command` | high |
| in_touch.c:2107-2157 | Gesture accumulator: two function-static floats persist swipe distance across calls only while console/message key-dest, self-described in-source as "absolutely horrible" | high |
| in_touch.c:2122-2135 | Same accumulator drives two console gestures off same `dy` sum: immediate `Con_Bottom()` at `dy>0.4f`, page-up/down repeat-fire at `±0.01` accumulated | high |
| in_touch.c:2148-2157 | Edge-swipe-to-exit-console: asymmetric thresholds (open zone 0.7/0.3 vs exit delta 0.1) | high |
| in_touch.c:2082-2098 | Rotation transform (swap+invert per `ref.rotation`) applied unconditionally at top of `IN_TouchEvent`, before menu/VGui/internal routing | high |
| in_touch.c:2100-2198 | Routing order: (1) menu-mouse-sim branch returns early (owns console/message accumulator, calls `Key_Event(K_MOUSE1,...)`); (2) VGui forwarding falls through (does NOT return); (3) touch enabled gate; (4) y rescaled by aspect ratio; (5) game-DLL `pfnTouchEvent` hook (can short-circuit); (6) `Touch_ControlsEvent` internal fallback | high |
| in_touch.c:2172-2187 | VGui forwarding does NOT consume — falls through even when active (unlike menu-mouse branch) | high |
| in_touch.c:2192 | `y` rescaled by `refState.height/width/Touch_AspectRatio()` right before game-DLL hook + internal routing — earlier branches see raw `y` | high |
| in_touch.c:2063-2078 | `Touch_ControlsEvent` precedence: edit-move owns event and returns; else edit-mode hit-test; else normal press; else (motion only) `Touch_Motion`; **always returns true** regardless of actual handling | high |
| in_touch.c:1841-1868,1901-1928 | Move/Look finger-collision self-heals via full-list release sweep; asymmetric logging (`Con_DPrintf` vs `Con_Printf`) | high |
| in_touch.c:1839-1847 | Second finger on move-type button silently reassigned back to existing move finger, not queued | high |
| in_touch.c:1274-1314 | `IN_TouchCheckCoords`: clamp min size → clamp [0,1] → optionally grid-snap; clamp-before-snap can push a snapped button slightly off-grid at edges (acknowledged TODO) | high |
| in_touch.c:1134-1157 vs 2266-2285 | Registration/teardown asymmetry: 24 registered, only 19 unregistered at shutdown (`touch_fade`, `touch_toggleselection`, `touch_aspectratio`, `touch_deleteprofile` never removed) | high |
| in_touch.c:484-523 | Two lookup families: `*NoPattern` exact match vs plain (glob-capable, `'*'`-detecting) — callers must pick the right one | high |
| in_touch.c:490,515 | `privileged` gate: unprivileged callers skip unprivileged-flagged buttons; privileged callers never filter by this bit | high |
| in_touch.c:823-824,839-840 | `Touch_AddButton`/`AddClientButton` privileged split: unprivileged forces `UNPRIVILEGED\|CLIENT` flags; `AddClientButton` additionally always forces `CLIENT\|NOEDIT` | high |
| in_touch.c:1044-1054 | `Touch_DeleteProfile_f` deletes via `FS_Delete` directly, bypassing list machinery (filesystem's surface) | high |
| in_touch.c:316-339 | `Touch_WriteConfig`: write-new/rotate-backup via 4 sequential `FS_Delete`/`FS_Rename` calls, no atomicity guard between them; short-circuits on `-nowriteconfig`/`!configchanged`/`!config_loaded` | high |
| in_touch.c:250-307 | `.cfg` format = sequence of `touch_*` commands, fixed dump order, `TOUCH_FL_CLIENT` buttons skipped | high |
| in_touch.c:213-241 | `Touch_ExportButtonToConfig`'s `keepAspect=true` path structurally dead within this file (no caller passes `true`) | med |
| in_touch.c:798-820 | `Touch_ReloadConfig_f` fallback path marks `configchanged=true` despite command name implying "not saving changes" | high |
| in_touch.c:1016-1041 | `Touch_EnableEdit_f` aspect-ratio reconciliation: two cases (A: reset ratio down; B: rescale button y-coords + clamp) via straight-line arithmetic, no rollback | high |
| in_osk.c:94-113 | First key at initial-enable state (`curbutton.val==0`) requires ENTER/A-button to "arm" cursor; arrows ignored until armed | med |
| in_osk.c:130-138 | SHIFT toggles layout 0/1 by parity of `curlayout`, immediately re-reads `curbutton.val` from new layout — shift is layout-swap, not modifier bit | high |
| in_osk.c:149-157 | Auto-unshift-on-release for ordinary chars only (not TAB/BACKSPACE/ENTER) | high |
| in_osk.c:159-163 | Char path always re-decodes as raw byte, conditionally UTF-8-force-processed; layout table is 7-bit ASCII only (Russian variants dead-commented) | high |
| in_osk.c:173-196 | Arrow-key wrap: UP/DOWN wrap resets `curbutton.val=0` (re-arm); LEFT/RIGHT wrap silently (no reset) | high |
| in_osk.c:213-224 | `OSK_EnableTextInput` only reseeds cursor state on `!old \|\| force` — repeated enable=true is a no-op on cursor state | high |
| in_osk.c:20 vs in_touch.c:22 | Compile-out shape differs: `in_touch.c` fully fenced by `#if !XASH_NO_TOUCH` with inline no-op stubs; `in_osk.c` has NO compile guard at all — always compiles, gated only by runtime `osk_enable` | high |
| in_keys.c:715 | OSK intercept unconditional regardless of `XASH_NO_TOUCH` — `osk.enable`/`osk_enable.value` is the only kill switch | high |

### 4.4 Touch button/grid data model + state machines

| legacy file:line | claim | confidence |
|---|---|---|
| in_touch.c:34-39 | `touchState`: `state_none`/`state_edit`/`state_edit_move` | high |
| in_touch.c:1002-1042,531-545,1995-2061,1612-1663 | State transitions: none→edit (`touch_enableedit`); edit→edit_move (button hit, if not NOEDIT); edit_move→edit (`IN_TouchEditClear`, on finger-up or `RemoveButtonFromList`); edit/edit_move→none (`touch_disableedit`, conditionally writes config if leaving via `key_game`) | high |
| in_touch.c:169-170 | `TO_SCRN_X/Y` macros: normalized [0,1]→pixels, Y additionally scaled by `Touch_AspectRatio()` | high |
| in_touch.c:1269-1272 | Grid computed live from `touch_grid_count` + current aspect ratio; `GRID_ROUND_X/Y` snap via `round()` | high |
| in_touch.c:186-198 | `Touch_AspectRatio()` resolution order: explicit config (≥0.25) > `actual_aspect_ratio` (≥0.25) > live `refState` compute > hardcoded 9/16 fallback | high |
| in_touch.c:176-184 | `Touch_NotifyResize` one-way ratchet toward taller ratios only (`<0.99` and increasing), gated on config-not-changed | high |
| in_touch.c:250-307 | `touch.cfg` full dump structure (header, sensitivity/grid/stroke/highlight/precise-look cvars, then `touch_setclientonly 0`+`removeall`+`aspectratio`+per-button `touch_addbutton`) | high |
| in_touch.c:822-854,872-925 | Two-tier default/profile model: `g_DefaultButtons[]` template list vs `touch.list_user` materialized copies via `Touch_LoadDefaults_f` | high |
| in_touch.c:2004 | Secondary sub-toggle: top-left grid-cell tap in `state_edit` toggles `showeditbuttons` independent of state machine | high |
| in_touch.c:2013-2041 | Selecting a `touch_command`-type button for edit re-links it to list tail (bring-to-front) — only for that type | high |

### 4.5 touch_* command census (~24)

| legacy file:line | claim | confidence |
|---|---|---|
| in_touch.c:1134-1157 | 24 commands: 9 unprivileged (`Cmd_AddCommand`: addbutton, removebutton, settexture, setcolor, setcommand, setflags, show, hide, fade), 15 restricted (`Cmd_AddRestrictedCommand`: enableedit, disableedit, list, removeall, loaddefaults, roundall, exportconfig, set_stroke, setclientonly, reloadconfig, writeconfig, deleteprofile, generate_code, toggleselection, aspectratio) | high |
| in_touch.c:1160-1187 | 22 `touch_*` cvars registered separately, plus `touch_enable` (defined in input.c, not this file) = 23 — count corrected 2026-07-20 by the campaign close-out audit | high |

### 4.6 Chunk-12 fence list (not analyzed further)

`ref.dllFuncs.GL_LoadTexture` (in_touch.c:1208,1431,1564);
`ref.dllFuncs.Color4ub` (1252,1367,1579,1586);
`ref.dllFuncs.R_DrawStretchPic` (1253,1342,1580,1587);
`ref.dllFuncs.GL_SetRenderMode` (1364,1434,1436,1505,1578);
`ref.dllFuncs.FillRGBA` (1453,1458,1463,1468,1484,1512,1514,1517,1520,1531,
1535,1547; in_osk.c:248,294);
`Con_DrawString` (in_touch.c:1489,1552; in_osk.c:270);
`CL_DrawCharacter` (in_osk.c:253);
`R_GetTextureParms`/`R_GetBuiltinTexture` (in_touch.c:1209,1331);
whole-function fences: `Touch_Draw` (1495), `Touch_DrawButtons` (1385),
`Touch_DrawTexture` (1247), `Touch_DrawCharacter` (1316),
`Touch_DrawText` (1348), `OSK_Draw` (in_osk.c:284),
`OSK_DrawSymbolButton` (238), `OSK_DrawSpecialButton` (266).

### 4.7 R10.4 Uncertainties

- `keepAspect=true` path unconfirmed dead outside this file's scope.
- `Touch_WriteConfig`'s non-atomic delete/rename failure modes not traced (filesystem's owned surface).
- Threading classification deferred to boundary-spec assembly: all touch/osk state touched exclusively via main/client input pump, no locks/atomics — Safe-TLS-equivalent under both legacy and target models; reasoned "none" for Race-shared/Signal-unsafe.
- Extension axes: reasoned "none" for G-1..G-5/P-1..P-8 as directly evidenced in these two files — pure event plumbing; P-3 (context-first) is the only axis arguably touched (heavy file-scope statics today), noted for orchestrator judgment, not asserted as a per-line claim.

---

## Part 5 — usercmd/input ABI recon (source: R10.5, `input.c` frame drivers + full ABI slot list)

Scope: `input.c` frame drivers, activation state machine,
`IN_EngineAppendMove` vs `IN_Commands` merge paths, `CL_CreateCmd` order,
`IN_JoyAppendMove`, `IN_LockInputDevices`, and the complete client/menu ABI
slot list touching input. All line numbers verified against HEAD at recon
time.

### 5.1 Interface

| legacy file:line | claim | confidence |
|---|---|---|
| input.c:437-459 | `IN_Init` registers cl_forwardspeed/backspeed/sidespeed unconditionally, then (non-dedicated) mouse/gyro/OSK/joy/touch/evdev startup | high |
| host.c:1132-1133 | `IN_Init()` runs in `Host_InitCommon`, immediately before `Key_Init()`, after `HPAK_Init()` | high |
| dedicated.c:57-60 | Dedicated build links no-op `IN_Init`/`Host_InputFrame` stub (separate TU, mutually exclusive at link time) | high |
| host.c:658-676 | `Host_Frame` calls `Host_InputFrame()` first, every frame, before client-begin/server/client-frame | high |
| input.c:649-654 | `Host_InputFrame` = `IN_Commands()` then `IN_MouseMove()`, unconditional | high |
| cl_main.c:698-701 | `CL_CreateCmd` order: `Platform_PreCreateMove()` → `clgame.dllFuncs.CL_CreateMove(...)` → `IN_EngineAppendMove(...)` — engine merge always AFTER client DLL's own move build | high |
| cl_main.c:643,680-691,936 | Usercmd destination: `cl.commands[i]` ring (`i = outgoing_sequence & CL_UPDATE_MASK`), or throwaway `nullcmd` during demoplayback | high |
| input.c:471-478,552-578 | `IN_JoyAppendMove` synthesizes `+forward`/`-forward`/etc. console commands from joystick axis thresholds (0.7 fwd/back, 0.9 side), tracked via persistent `static uint moveflags` | high |
| input.c:552-578 | `IN_CollectInput(forward,side,pitch,yaw,includeMouse)`: reads `Platform_MouseMove` only if `includeMouse`; always folds gyro/joy/touch finalize; applies `look_filter` 2-tap average vs `static inputstate` | high |
| input.c:92-108 | `IN_LockInputDevices(lock)` sets/clears `FCVAR_READ_ONLY` on `m_ignore`, `joy_enable` (extern), `touch_enable` — freezes device-connect surface once player connected | high |

### 5.2 Merge math: IN_EngineAppendMove vs IN_Commands (modern path)

| legacy file:line | claim | confidence |
|---|---|---|
| input.c:587-615 | `IN_EngineAppendMove` bypasses entirely (returns) if client DLL implements `pfnLookEvent` — "modern path" opt-out | high |
| input.c:599-614 | Legacy-path merge: `IN_CollectInput(..., includeMouse=false)` — mouse deltas explicitly NOT read here; joystick via `IN_JoyAppendMove`; yaw/pitch added to `cmd->viewangles` with hard `bound(-90,pitch,90)` clamp, `cl.viewangles` mirrored | high |
| input.c:617-634 | `IN_Commands` modern-path driver: `pfnLookEvent` set → `IN_CollectInput(..., includeMouse = in_mouseinitialized && !m_ignore.value)`; if `key_dest==key_game`, calls `pfnLookEvent(yaw,pitch)` then `pfnMoveEvent(forward,side)` directly — no engine-side clamp or mirroring, client DLL owns that math | high |
| input.c:636-639 | `IN_Commands` tail always re-syncs mouse grab/relative-mode via `IN_CheckMouseState(in_mouseactive)`, independent of look path | high |

### 5.3 Mouse activation state machine

| legacy file:line | claim | confidence |
|---|---|---|
| input.c:174-210 | `IN_ToggleClientMouse(newstate,oldstate)`: no-op if equal; sets cursor/evdev grab first, bails if `m_ignore.value` (cursor still updated, mouse never (de)activated); else activates on transition into `key_game`, deactivates on transition out | high |
| input.c:271-293 | `IN_CheckMouseState(active)`: Win32 raw-input gate `(m_rawinput.value && client_dll_uses_sdl) \|\| pfnLookEvent != NULL`; non-Win32 always SDL relative mode; `m_ignore.value` forces `active=false`; relative-mode requires `active && use_raw_input && !mouse_visible && ca_active`; grab requires `active && !mouse_visible && ca_active` (no raw-input dep) | high |
| cl_game.c:4018-4019 | `client_dll_uses_sdl` set once at client-DLL load, via direct-dependency probe on `SDL2.<ext>` — not live/per-frame | high |
| input.c:302-311,320-329 | `IN_ActivateMouse`/`DeactivateMouse` both early-return if `!in_mouseinitialized`; both call `IN_CheckMouseState` then optional client-DLL callback, then set `in_mouseactive` | high |
| input.c:212-246,248-269 | `IN_SetRelativeMouseMode`/`IN_SetMouseGrab` edge-triggered via file-local `static qboolean` latches (`s_bRawInput`, `s_bMouseGrab`) — same-value calls no-op | high |

### 5.4 IN_LockInputDevices READ_ONLY flip quirk

| legacy file:line | claim | confidence |
|---|---|---|
| input.c:92-108 | Mutates cvar `flags` bitfields directly for 3 cvars (not via cvar-set path) — pure flag toggle, values untouched, fully reentrant/idempotent | high |

### 5.5 External ABI contracts

**`cdll_exp.h` (client-DLL exports, engine calls into client)**

| legacy file:line | claim | confidence |
|---|---|---|
| cdll_exp.h:33-86 | `cldll_func_t` carries: `IN_ActivateMouse`, `IN_DeactivateMouse`, `IN_MouseEvent(int mstate)`, `IN_ClearStates`, `IN_Accumulate`, `CL_CreateMove(float,usercmd_s*,int)`, `KB_Find(const char*)` (returns `void*`), `pfnKey_Event(int,int,const char*)`, plus FWGS extensions `pfnTouchEvent`, `pfnMoveEvent`, `pfnLookEvent` | high |
| cdll_exp.h:49-52,80-82 | "NOTE: ordering is important!" — ABI-versioned vtable, not named-lookup; `KB_Find` returns opaque `void*` | high |
| cdll_exp.h:80-82 | Touch/Move/Look explicitly "Xash3D FWGS extension" — not original GoldSrc surface | high |

**`cdll_int.h` (engine-provided, client calls into engine)**

| legacy file:line | claim | confidence |
|---|---|---|
| cdll_int.h:187-189 | `Key_Event(int,int)`, `GetMousePosition(int*,int*)` in "Added for user input processing" block | high |
| cdll_int.h:219 | `Key_LookupBinding(const char*)` — reverse lookup | high |
| cdll_int.h:265-267 | `pfnGetMousePos(tagPOINT*)`, `pfnSetMousePos(int,int)`, `pfnSetMouseEnable(qboolean)` — 3 consecutive slots | high |
| cl_game.c:2900-2911,3865-3867 | `pfnGetMousePos` real forwarder to `Platform_GetMousePos`; `pfnSetMousePos` wired directly to `Platform_SetMousePos` — neither is a stub | high |
| cl_game.c:2913-2922 | `pfnSetMouseEnable` is **the one** function with empty body + "legacy of dinput code" comment — the only verified dinput stub in this surface (brief's "two dinput stubs" not corroborated by repo-wide grep) | high |
| cdll_int.h:311 | `CLDLL_INTERFACE_VERSION 7` | high |

**`menu_int.h` (engine/menu ABI)**

| legacy file:line | claim | confidence |
|---|---|---|
| menu_int.h:59-180 | `ui_enginefuncs_t` (engine→UI): `pfnKeyClearStates`, `pfnSetKeyDest(int)`, `pfnKeynumToString`, `pfnKeyGetBinding`, `pfnKeySetBinding`, `pfnKeyIsDown`, `pfnKeyGetOverstrikeMode`/`pfnKeySetOverstrikeMode`, `pfnKeyGetState` | high |
| menu_int.h:182-200 | `UI_FUNCTIONS` (UI→engine): `pfnKeyEvent(int,int)`, `pfnMouseMove(int,int)`, `pfnGetCursorPos`/`pfnSetCursorPos`, `pfnShowCursor(int)`, `pfnCharEvent(int)`, `pfnMouseInRect(void)` | high |
| cl_gameui.c:67-77,131-135 | `UI_KeyEvent`/`MouseMove`/`CharEvent` thin engine-side wrappers, gated `gameui.hInstance`, forwarding to `UI_FUNCTIONS` vtable | high |
| in_keys.c:790-830 | Key routing precondition: `key_game` + `mouse_visible` diverts entirely to client DLL and returns before menu code; `key_menu` gets optional `UI_CharEvent` (only if lacking extended API) before `UI_KeyEvent` always fires | high |
| input.c:355-358 | `IN_MouseMove` forwards raw mouse position to both `VGui_MouseMove` and `UI_MouseMove` unconditionally; touch-emulation short-circuits both via `Touch_WantVisibleCursor()` | high |

### 5.6 Threading (R10.5)

Legacy T_Main-only throughout; xash3dpp target is also T_Main per the
ratified model — classification below is about in-process hazard shape
(re-entrancy/lazy-init ordering), not cross-thread migration risk.

| legacy file:line | claim | class |
|---|---|---|
| input.c:29-40 | Module statics (`in_mouseactive`, `in_mouseinitialized`, `in_lastvalidpos`, `in_mouse_savedpos`, `in_mstate`, `inputstate`) — file-scope, touched only from `Host_Frame`'s call chain on T_Main | Safe-TLS (legacy); would become Race-shared if a future thread called into input frame code |
| input.c:214,250 | `s_bRawInput`/`s_bMouseGrab` function-local static edge latches | Race-static-buf shape, Safe-TLS in practice — would race under multi-thread input dispatch |
| input.c:480 | `IN_JoyAppendMove`'s `static uint moveflags` — persistent frame-to-frame edge state | Race-static-buf shape — must become owned member state if collect/append split |
| input.c:92-99 | `IN_LockInputDevices` plain `SetBits`/`ClearBits`, no atomics | Safe under T_Main-only cvar access; Race-shared otherwise |
| cl_game.c:4018 | `client_dll_uses_sdl` set once at load, read every frame | Race-lazy-init shape, benign under T_Main-only load+frame sequencing (med confidence) |

### 5.7 R10.5 Uncertainties

- Brief's "two dinput stubs" not corroborated — only `pfnSetMouseEnable` carries the explicit dinput-legacy comment/empty body; repo-wide grep for `dinput`/`DirectInput` found no second match.
- `IN_EngineAppendMove`'s `includeMouse=false` means the legacy (non-`pfnLookEvent`) engine-merge path never reads `Platform_MouseMove` deltas — mouse look must be entirely client-DLL-internal for that path; the client-DLL-side half of this claim is inferred, not directly cited (HL SDK client source out of this recon's file scope).
- Extension axis set was re-read live at recon time but this fragment does not evaluate any axis against the material — left for orchestrator adjudication rather than a forced or reasoned-none claim.

---

## Cross-fragment notes (assembly-pass observations, not new claims)

- **`Key_Event` call chain spans R10.1 and R10.5**: R10.1 documents the full
  13-step internal routing; R10.5 documents the external ABI slots
  (`pfnKey_Event`, `pfnKeyEvent`) that step 5/11 dispatch into. Both agree
  on the client-DLL-first-refusal semantics; no contradiction found.
- **`IN_ToggleClientMouse` appears in both R10.1 (in_keys.c:883-886 call
  site) and R10.5 (input.c:174-210, the function body itself)** — R10.1
  flags a potential reentrancy hazard from the old/new-dest argument
  ordering; R10.5's own read of the function body doesn't independently
  flag that hazard. Not a contradiction — different vantage points on the
  same function, both cite consistent line ranges.
- **`Platform_GetMousePos`/`SetMousePos` appear in R10.2 (platform seam
  classification) and R10.5 (ABI slot: `pfnGetMousePos`/`pfnSetMousePos`
  forward to them directly)** — consistent: R10.2 establishes these are
  the only two `GAME_EXPORT` functions in the input-relevant `Platform_*`
  set; R10.5 confirms the `cdll_int.h` slots that expose them.
- **Joystick/gyro (R10.3) and the frame-driver merge (R10.5) meet at
  `input.c:566-567`** (`IN_GyroFinalizeMove` → `Joy_FinalizeMove` →
  `Touch_GetMove`, all additive into the same fw/side/pitch/yaw): R10.3
  supplies the internals of the first two calls, R10.5 supplies the
  calling context (`IN_CollectInput`) and its `includeMouse` gating. No
  contradiction.
