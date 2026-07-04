# xash3dpp/tests — conventions & shared fixtures

One page so no session re-derives the harness by grepping. Registration
lives in each `tests/<subsystem>/CMakeLists.txt` via
`xash3dpp_add_test(name [SOURCE path] LIBS ...)`; subfolders under a
subsystem are registered from that one file by relative source path (no
nested CMakeLists).

## Conventions (every test executable)

- `#include "../../test_helpers.hpp"` (adjust depth) — `CHECK`,
  `CHECK_EQ/NE/LT/LE/STREQ`, fatal `REQUIRE`, `RUN_TEST`; suppresses
  MSVC CRT dialogs so debug asserts reach CTest.
- File scope: `static int g_pass = 0, g_fail = 0;`; `main()` ends with
  the `<name>: %d passed, %d failed` print and returns `g_fail != 0`.
- Any test that touches a subsystem asserting thread roles must open
  `main()` with
  `xash::core::register_thread_role( xash::core::ThreadRole::Main );`
  (symptom otherwise: instant `STATUS_BREAKPOINT`, output "thread role
  mismatch: expected Main, got Unknown").
- Debug builds assert pool cleanliness in `destroy_pool` — fixtures must
  tear down owners before pools (leak-asserts are part of the test).
- No committed binary fixtures: byte images are synthesized in-test
  (`tests/filesystem` write_pak/write_zip/write_wad precedent) and
  written to a `std::filesystem::temp_directory_path()` tree that the
  test removes at exit.

## Shared fixtures (reuse, don't reinvent)

| Fixture | Where | What it gives you |
|---|---|---|
| `test_helpers.hpp` | `tests/` | the macro set above |
| `TestBspBuilder` + `make_minimal_world()` | `tests/map_loader/bsp/test_bsp_builder.hpp` | in-memory BSP images; `make_minimal_world()` is a complete valid v30 world (hull0 WATER x<128, hull1 SOLID x<128&&y<64, clipnode face at x=128); `set_entities()` overrides the entity lump; BSP30ext / Blue-Shift lump-swap switches |
| cmd_cvar stubs | `tests/cmd_cvar/test_stubs.hpp` + STATIC lib target `cmd_cvar_test_stubs` | `TrustedOracle` / `UntrustedOracle` / `NullPolicy` / `make_test_context()` — link the lib, don't recompile the stubs |
| Fake game DLL | `tests/server/abi/fake_game_dll.cpp` (+ `fake_dll_state.hpp`) | MODULE targets `fake_game_dll_{full,legacy,nohandshake,badver}`; consume via `FAKE_DLL_*="$<TARGET_FILE:...>"` defines + `add_dependencies`; `fake_state` export exposes handshake order, counters, version echoes; probes: cross-DLL engine callback run, GameShutdown cvar write, `on_free_out` pointer (DLL increments TEST-owned memory so unload-time callbacks stay observable) |
| Bridge fixture | `tests/server/abi/test_engine_table.cpp` | the assemble-everything pattern for engine-table tests: pool + arena + strings + minimal world + links/move env + bridge install; claims world+client edicts like `SV_SpawnServer` does (a fresh arena is all-free) |
| Real-FS temp tree | `tests/map_loader/test_map_loader_world.cpp`, `tests/server/lifecycle/test_game_lifecycle.cpp` | `fs.init(root, "valve", "game")` + `add_game_directory(root/"game", SearchPathFlags::GameDir)`; drop `maps/*.bsp`, `delta.lst`, `.ent` patches into the tree |
| Minimal `delta.lst` script | `tests/networking/delta/test_delta_tables.cpp` (`k_script`) | the 3-field `event_t` table both delta tests and the server lifecycle fixture use |
| `FakePlatformSockets` | `tests/networking/` | socket seam for NetworkContext tests (no real network) |

## Goldens convention

Hand-derived expected values, cross-checked against verbatim-legacy
harnesses where the chunk has one (Q-18; see
`docs/design/decisions-architecture.md`). Never copy a golden from the
implementation's own output.
