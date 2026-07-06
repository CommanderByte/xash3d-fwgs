# abi — architecture

`abi` is the frozen game-DLL ABI **vendoring + bridge** layer. It contains no
engine logic and no runtime state: it vendors byte-exact C++ mirrors of the
GoldSrc/Xash SDK structs, enums, function-pointer tables, and constants, and
hosts the `extern "C"` direct-export shims (`Host_Error`, …) that game DLLs link
against by name. Full contract: [`../../boundaries/abi-boundary.md`](../../boundaries/abi-boundary.md).

## Layout

| File | Contents |
|---|---|
| `include/xash3dpp/abi/abi_types.hpp` | width-frozen primitive typedefs + `color24` |
| `include/xash3dpp/abi/edict.hpp` | `entvars_t` / `edict_t` / `globalvars_t` / `link_t` |
| `include/xash3dpp/abi/eiface.hpp` | `enginefuncs_t` / `DLL_FUNCTIONS` / `NEW_DLL_FUNCTIONS` + support PODs |
| `include/xash3dpp/abi/pm_defs.hpp` | pmove working set (`playermove_t`, `physent_t`, …) |
| `include/xash3dpp/abi/entity_state.hpp` · `usercmd.hpp` · `weaponinfo.hpp` · `event_args.hpp` · `event_state.hpp` · `pm_movevars.hpp` | HLSDK exchange/wire PODs |
| `include/xash3dpp/abi/server_consts.hpp` | ABI/wire-frozen `k_*` constants (QO) |
| `include/xash3dpp/abi/engine_context_accessor.hpp` | `current_/set_current_engine_context` (Q-2 exception) |
| `src/abi/engine_funcs.cpp` | `extern "C"` `GAME_EXPORT` bridge (`Host_Error`) |

Layout parity is pinned by `static_assert`s in the headers and by
`tests/server/abi/test_*_layout.cpp` on both 32- and 64-bit targets.

## Threading

`abi` declares layout and hosts a thin bridge; it owns **no** state, so the
thread-role assertion policy has no mutating entry point to guard *inside* this
layer.

- **Vendored headers** are pure declarations — thread-safety is the
  caller/engine contract, not the header's (declared via the file-scope
  `@annotation-exempt: abi-pod` markers).
- **`current_engine_context()`** is a lock-free `std::atomic` **acquire** load,
  callable from any C-ABI caller thread (game-DLL threads included).
  **`set_current_engine_context()`** is a **release** store made
  main-thread-only (called once each from `EngineContext::init`/`shutdown`); its
  state and the corresponding `assert_thread_role(ThreadRole::Main)` wiring live
  in `host`.
- **`Host_Error`** is intentionally callable from any game-DLL thread and does
  not assert a role; the main-thread / recursion policy is enforced downstream
  in `Host::signal_frame_abort` (host Q-4 / OQ-1).

## Non-goals

No allocation, no stats/instrumentation (state-free — `stats exempt`), no
`limits.hpp` entries (all constants are ABI-frozen `k_*`, QO). The
`enginefuncs_t` slot *implementations* are `server`'s, not `abi`'s.
