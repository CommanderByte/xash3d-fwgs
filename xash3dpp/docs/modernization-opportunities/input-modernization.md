# input Modernization Opportunities

> C++ standard in use: C++**23** (`xash3dpp/CMakeLists.txt:7`
> `CMAKE_CXX_STANDARD 23`; `xash3dpp/src/input/CMakeLists.txt:32`
> `target_compile_features(xash3dpp_input PUBLIC cxx_std_23)`).
> Boundary spec: `docs/boundaries/input-boundary.md` (Chunk 10, as-built
> 2026-07-20).
> ABI-frozen symbols in this subsystem: **none vendored today.** Input
> drives three frozen inter-DLL vtables — `cldll_func_t`
> (`engine/cdll_exp.h`), `cl_enginefunc_t` (`engine/cdll_int.h`),
> `ui_enginefuncs_t` (`engine/menu_int.h`) — but none of their slot
> signatures are implemented by Chunk 10; they are recorded in the
> boundary spec purely as the Chunk-12 vendoring target. The only
> **live** compat-frozen surface is the `keynames[]` name/keynum table,
> the `bind "<name>" "<binding>"` config-line format, and the
> `touch_addbutton`-style command-script format (input-boundary.md
> "Compat scope (Q-12)") — none of these are touched by any finding
> below.
>
> **2026-07-20**: initial creation for the tree-wide modernization audit
> (Phase 2). No prior `input-modernization.md` existed. Findings below
> are drawn from the Phase-2 `input` pack (digest IDs `F136`-`F142`),
> cross-checked against HEAD, plus two mechanical false positives found
> during that cross-check (see "Investigated and refuted"). `F139`
> (High, main-thread-split classification) was **REFUTED** in Phase 2
> and is not repeated here.

## Summary

Chunk 10 (`xash3dpp_input`, 9 source TUs / 9 public + 5 private headers /
6 test executables) was written directly in modern C++, not converted
from the legacy `in_*.c` family — `std::optional`, `std::string_view`,
`std::array`, `enum class`, and pimpl are used throughout, and every
mutating entry point on the public `Input` class already asserts
`ThreadRole::Main`. There is no legacy-C residue at the class-surface
level; the residue that remains is narrow and specific: two structs
still model a 3/4-component vector as a raw C array next to sibling
classes in the same header that already use `std::array` for the
identical shape (M-1, M-2), and one interface method — genuinely
zero-caller today, so free to change — still returns through a C-style
`char *`/size out-buffer instead of a value type (M-3).

The tree-wide "13 interfaces with zero production implementations"
headline includes input's `IEventSource` and `IWindowControls`, but
both are **already correctly recorded** as Chunk-12 vendoring doors in
the boundary spec (not over-abstraction) — see "Out of scope" below.
`InputStats` is already **fully atomic** (6 always-on + 2
`XASH_STATS`-gated fields, all `std::atomic`); the Phase-0 fact base's
`atomic_fields: 1` for `InputStats` was a regex undercount (see
`lenses.md` "Atomic (safe): ... InputStats (8)") — no action needed,
input already clears its own G-3/P-2 cross-thread-read bar. Similarly,
the subsystem's 9 `compliance-allow(thread-assert)` waivers are one
uniform, verified pattern (a private `Impl`-layer function reachable
only through a public `Input::` method that already asserted `Main`)
and were confirmed tree-wide as the majority shape of all 62
thread-agnostic waivers in the tree — not debt, no action needed.

The remainder of this report is a small tail of mechanical hygiene
items ([[nodiscard]], `NS_QUALIFY`, integer width, Q-13 pre-reserve
annotations) surfaced by `compliance_scan input`, plus one CMake
over-linkage fix. Two of that scanner's own hits are false positives
specific to this subsystem and are recorded, not repeated, under
"Investigated and refuted" so nobody re-files them.

______________________________________________________________________

## High-priority opportunities

None found this pass. The one candidate the Phase-2 pack raised at High
— splitting Input's thread-pinning into a by-design SDL-affinity core
vs. an incidental routing/math core, as a document to prepare for a
future off-main input thread — was **REFUTED**: it contradicts the
ratified thread model (`input-boundary.md` "Threading": *"input =
T_Main, full stop — no NetIO/AudioDecoder-style split exists or is
planned"*) and the boundary spec's own P-1 row already reasons the same
case through to the opposite conclusion. See "Investigated and
refuted".

## Medium-priority opportunities

### M-1: `GyroCalibration` uses raw C arrays where its two siblings in the same header already use `std::array`

- **File(s)**: `xash3dpp/include/xash3dpp/private/input/joy_model.hpp:162,164`
  (declarations); 9 usage sites in
  `xash3dpp/src/input/joy/joy_model.cpp:216,229,234,253-255,262,270,283`
- **Current pattern**: `GyroCalibration` (joy_model.hpp:138-167) declares
  `float sum_[3]` and `float bias_[3]`. `JoyModel` and `DeviceGyro`,
  immediately above it in the same header, already model the identical
  3-component-float-vector concept as `std::array<float, 3>`
  (`axis_map_`/`gyro_speed_`/`gyro_display_` at joy_model.hpp:101-104,
  `speed_` at joy_model.hpp:123) and already do whole-array
  aggregate-init (`gyro_speed_{ 0.0f, 0.0f, 0.0f }`) instead of the
  per-element statements `sum_[0] = sum_[1] = sum_[2] = 0.0f;` that
  `GyroCalibration` still writes.
- **Suggested replacement**: `std::array<float, 3> sum_{}, bias_{};`.
  The 9 usage sites in `joy_model.cpp` become aggregate init/assignment
  matching the sibling classes' existing style two lines above them;
  indexed reads (`sum_[0]`, `bias_[1]`, …) are unchanged since
  `std::array::operator[]` has the identical syntax.
- **Boundary-safe**: Yes. `GyroCalibration` is pure internal engine
  state (the gamepad-gyro calibration state machine, Quirk 7) with no
  wire format and no vendored ABI struct.
- **Rationale**: Removes the one remaining raw-array outlier in a
  header that otherwise already completed this exact conversion;
  brings whole-array copy/compare/aggregate-init for free and matches
  the pattern the two neighbouring classes already establish, so a
  future reader does not have to ask why one of the three is
  different.

### M-2: `TouchButtonRecord`/`TouchDefaultButtonRecord`/`TouchButtonDesc` use raw `std::uint8_t color[4]` with manual element-copy loops

- **File(s)**: `xash3dpp/include/xash3dpp/private/input/touch_model.hpp:28,45`;
  `xash3dpp/include/xash3dpp/input/touch.hpp:95` (struct fields);
  `xash3dpp/include/xash3dpp/private/input/touch_model.hpp:94,114,117`
  (parameter declarations); copy loops at
  `xash3dpp/src/input/touch/touch.cpp:197,458` and
  `xash3dpp/src/input/touch/touch_model.cpp:71,202`
- **Current pattern**: three struct definitions declare
  `std::uint8_t color[4]`; three function parameters take
  `const std::uint8_t color[4]`; four call sites manually copy
  element-by-element: `for (int i = 0; i < 4; ++i) { b.color[i] =
  color[i]; }`.
- **Suggested replacement**: `std::array<std::uint8_t, 4> color{ 255,
  255, 255, 255 };` for all three struct members, and
  `const std::array<std::uint8_t, 4> &color` for the parameter types.
  The four manual copy loops collapse to a single `b.color = color;`
  assignment each.
- **Boundary-safe**: Yes. `TouchButtonRecord`/`TouchButtonDesc` are
  purely internal engine state — the legacy `touch_button_t` is
  explicitly **not** vendored in Chunk 10 (input has no netchan/ABI
  wire format of its own per the boundary spec's Compat scope
  section). The one place a raw byte layout matters is the
  `touch_addbutton` command-script text emitted by
  `touch_model.cpp:636` (`%d %d %d %d`, still indexable via
  `operator[]` on a `std::array`), so the compat-format contract is
  unaffected.
- **Rationale**: Eliminates four hand-rolled copy loops in favour of
  one aggregate assignment each, and gives the color triple/quad
  value semantics (comparable, default-constructible, `.size()`)
  consistent with the rest of the touch data model.

### M-3: `IWindowControls::get_clipboard_text` still uses a C-style out-buffer + size return shape, and has zero callers today — free to fix now

- **File(s)**: `xash3dpp/include/xash3dpp/input/window_controls.hpp:44-45`
  (interface), `:59-64` (the only implementation, `NullWindowControls`)
- **Current pattern**:
  `[[nodiscard]] virtual std::size_t get_clipboard_text(char *buf,
  std::size_t buf_size) const noexcept = 0;` — a classic
  `strlcpy`-style output-buffer API, the one C-shaped member on an
  otherwise all-modern interface (every sibling member is a clean
  value-returning or `bool`-returning `noexcept` virtual). Confirmed
  zero call sites anywhere in `src/` or `tests/` — the only
  implementation is `NullWindowControls`, a documented no-op stub
  (`window_controls.hpp:50` `XASH3DPP-STUB(chunk12)`), and
  `INP-OQ-3` (input-boundary.md) records that its clipboard placement
  is itself unresolved, re-deferred to the eventual window/platform
  split.
- **Suggested replacement**:
  `[[nodiscard]] virtual std::optional<std::string>
  get_clipboard_text() const noexcept = 0;` (table row 2-H) — update
  `NullWindowControls::get_clipboard_text()` to `return std::nullopt;`.
  `set_clipboard_text(const char *)` can stay as-is (a view-only
  in-parameter is already idiomatic) or move to `std::string_view` at
  the same time for consistency.
- **Boundary-safe**: Yes. No production caller exists to break, and
  the interface is not part of any frozen ABI table — `cl_enginefunc_t`
  and friends are a separate, unvendored surface this class will back
  later, not this method's own shape.
- **Rationale**: `[EXT:G-2]` — this is exactly the kind of
  context-less C-ABI-shaped surface G-2's eventual game-ABI v2 rework
  targets; input's own internal `IWindowControls` surface should stay
  clean now rather than accrete a second consumer of the old shape
  once the real SDL backend (Chunk 12) lands. Fixing it today costs
  nothing (zero call sites to migrate); fixing it after a Chunk-12
  backend exists costs a signature change plus a real caller update.
  Extension-bumped from Low to Medium per the campaign's tier rule.

## Low-priority / cosmetic opportunities

### L-1: Six `[[nodiscard]]`-missing declarations on non-void private accessors

- **File(s)**: `xash3dpp/include/xash3dpp/private/input/input_impl.hpp:208`
  (`joy_tunables()`), `:209` (`joy_tunables_peek()`), `:210`
  (`device_gyro_tunables()`), `:214` (`touch_tunables()`), `:215`
  (`touch_event(...)`); `xash3dpp/include/xash3dpp/private/input/touch_model.hpp:172`
  (`edit_hit_test(...)`)
- **Current pattern**: six non-`void`-returning `Impl`/`TouchModel`
  member declarations without `[[nodiscard]]`, against the repo
  convention that it is the default for non-void returns.
- **Suggested replacement**: add `[[nodiscard]]` to all six
  declarations.
- **Boundary-safe**: Yes — private-header-only, no ABI surface.
- **Rationale**: Mechanical convention compliance
  (`compliance_scan input`, `nodiscard-missing`, 6/6 findings verified
  against HEAD); catches an accidentally-discarded query result at
  compile time.

### L-2: Ten sibling-namespace references not absolute-qualified (`NS_QUALIFY`)

- **File(s)**: `xash3dpp/include/xash3dpp/input/input.hpp:317,318`;
  `xash3dpp/include/xash3dpp/private/input/input_impl.hpp:132,134`;
  `xash3dpp/src/input/input.cpp:18,48,56,62,71,72`
- **Current pattern**: bare `memory::PoolHandle`/`memory::mem_alloc`/
  `memory::mem_free` references to the sibling `memory` namespace
  instead of the repo's required `::xash::memory::` absolute form.
- **Suggested replacement**: qualify each site as `::xash::memory::PoolHandle`,
  `::xash::memory::mem_alloc`, `::xash::memory::mem_free`.
- **Boundary-safe**: Yes — pure namespace-qualification style, no
  behavioural or ABI change.
- **Rationale**: Repo-wide `NS_QUALIFY` convention
  (`.github/instructions/xash3dpp.instructions.md`); 10/10 sites
  verified against HEAD via `compliance_scan input`.

### L-3: Two bare `unsigned` uses where the repo convention wants a width-qualified type

- **File(s)**: `xash3dpp/src/input/keys/key_routing.cpp:206`;
  `xash3dpp/src/input/touch/touch_model.cpp:637`
- **Current pattern**: `in_mstate &= ~static_cast<unsigned>(1u <<
  button);` and `static_cast<unsigned>(b.flags)` — bare `unsigned`
  instead of a width-qualified type such as `std::uint32_t`.
- **Suggested replacement**: `static_cast<std::uint32_t>(...)` at both
  sites (`in_mstate`'s declared type and the `%u` `std::snprintf`
  format argument both already assume 32-bit width).
- **Boundary-safe**: Yes.
- **Rationale**: Cosmetic width-qualification convention
  (`decisions-architecture.md` INT_TYPES); no observable behaviour
  change on any currently-shipping arch (x86/x64 `unsigned` is already
  32-bit), so this is style-only, not a portability fix.

### L-4: Four hot-path out-vector parameters/members missing the Q-13 `@pre-reserved` annotation (or a cold-path exemption note)

- **File(s)**: `xash3dpp/include/xash3dpp/private/input/input_impl.hpp:211`
  (`apply_joy_key_transitions(std::vector<detail::JoyKeyTransition> &t)`),
  `:216` (`dispatch_touch_commands(std::vector<detail::TouchModel::CommandDispatch>
  &cmds)`); `xash3dpp/include/xash3dpp/private/input/touch_model.hpp:134`
  (`collect_commands(..., std::vector<CommandDispatch> &out_commands)`),
  `:224` (`default_buttons_` member)
- **Current pattern**: four `std::vector<T>` out-parameters/members on
  paths called from the per-frame input pump
  (`apply_joy_key_transitions` from `Input::run_commands`,
  `dispatch_touch_commands`/`collect_commands` from the touch event
  path) with no `.reserve()` call and no `// @pre-reserved: <LIMIT>`
  annotation — unlike the sibling `keys_` vector in
  `key_table.hpp:94`, which already carries one
  (`@pre-reserved: input_key_count`).
- **Suggested replacement**: either add a bound + `.reserve()` at
  construction/reset (e.g. joystick key transitions are bounded by the
  joystick axis/button count; touch command dispatch is bounded by the
  active touch-button count) with a matching `@pre-reserved: <LIMIT>`
  comment, or — if a judgment call concludes these are warm/cold-path
  (per-event, not per-frame-unconditionally) — add the explicit
  cold-path exemption comment the convention calls for instead of
  leaving the annotation silently absent.
- **Boundary-safe**: Yes — pure internal allocation-policy hygiene.
- **Rationale**: `NeedsVerification` on hot-vs-cold classification
  (the compliance tool itself flags this check as judgment-required,
  not a hard rule); recorded here so the classification is made
  explicitly rather than left as an unannotated gap. Four candidate
  sites, verified against HEAD.

### L-5: `xash3dpp_input` links `xash3dpp_cmd_cvar` `PUBLIC` when only its private headers use it

- **File(s)**: `xash3dpp/src/input/CMakeLists.txt:36`;
  `xash3dpp/include/xash3dpp/input/input.hpp:34` (only forward-declares
  `CmdCvarContext` for a borrowed-pointer member); `xash3dpp/include/xash3dpp/private/input/input_impl.hpp:16-17`
  (the actual `cmd_cvar/context.hpp`/`cvar.hpp` includes, confined to
  `src/input/**/*.cpp`'s private header)
- **Current pattern**: `target_link_libraries(xash3dpp_input PUBLIC
  xash3dpp_cmd_cvar ...)` — no public `input/` header includes a
  `cmd_cvar` header, only the private header does.
- **Suggested replacement**: `PRIVATE xash3dpp_cmd_cvar`. For a
  `STATIC` library, CMake still forwards `PRIVATE` link dependencies to
  the final executable's link line via `LINK_ONLY`, so `input.cpp`'s
  calls into `CmdCvarContext` still resolve — this stops leaking the
  include-directory usage requirement to consumers who only use
  `Input`'s public header, matching the treatment `xash3dpp_core`
  already gets in the same file.
- **Boundary-safe**: Yes — build-graph-only change, no source edit.
- **Rationale**: Keeps the public/private dependency surface honest;
  a consumer linking only `xash3dpp_input`'s public API should not
  transitively gain `cmd_cvar`'s include directories.

______________________________________________________________________

## Out of scope / ABI-frozen

- **`cldll_func_t` / `cl_enginefunc_t` / `ui_enginefuncs_t` slot
  shapes** (input-boundary.md "External ABI contracts") — frozen by
  GoldSrc/FWGS ABI contract once vendored; recorded in the boundary
  spec purely as the Chunk-12 target shape. Not vendored today, so
  there is nothing in the current tree to modernize or regress.
- **`keynames[]` name/keynum table, `bind` config-line quoting,
  `touch_*` command-script emission order/format** — Q-12 compat
  surface (input-boundary.md "Compat scope"); any change to string
  content, quoting, or emission order is an observable-behaviour
  regression against mods that regex-parse `config.cfg`/`touch.cfg`,
  not a modernization target.
- **`IEventSource` (15 pure virtuals) / `IWindowControls` (7 pure
  virtuals) — zero production implementations.** Both are correctly
  **already recorded** as Chunk-12 vendoring doors in
  `input-boundary.md`'s Interface section (explicit "Chunk-12
  vendoring target" language) and Extension-axes table (P-4/G-1/G-5
  rows), with `MockEventSource`/`NullWindowControls` as the interim
  test/null-backed implementation. This is the tree-wide L11-subtraction
  finding's "scheduled doors — keep, RECORD them" bucket, not the
  "delete" bucket (`ICvarObserver` is the tree's one genuine
  over-abstraction; input has none). No action item follows from this
  for a *modernization* report — see "Open questions" for the one
  outstanding doc-formatting gap.
- **`static tinystr[16]` reuse hazard already eliminated** —
  `Key_KeynumToString`'s legacy shared static return buffer
  (in_keys.c:229) is called out in the boundary spec's Owned-state and
  Threading tables as a hazard that "must not survive the port"; it
  did not — `keynum_to_string()` already returns
  `std::optional<std::string>` by value (`input.cpp:197-203`). Recorded
  here as a closed item, not a re-discoverable finding.

## Open questions

- **`IEventSource`'s Q-21 door row is implicit, not explicit.**
  `input-boundary.md`'s Extension-axes table documents `IEventSource`
  as the G-1/G-5 injection seam and `IWindowControls` throughout the
  Interface section as a "Chunk-12 vendoring target", but neither
  carries the literal one-line "interface X: chunk N supplies the
  production implementation; zero today" row the tree-wide
  `[SUB-5]` recommendation (`lenses.md`) asks every boundary spec with
  a zero-impl interface to carry. This is a **boundary-spec
  documentation** fix (`docs/boundaries/input-boundary.md`), out of
  this modernization report's file scope — flagged here so the doc
  owner does not have to re-derive that input is one of the five
  boundary specs `[SUB-5]` names (alongside server, content,
  networking, save).
- **P-4 snapshot types (`BindingEntry`, `TouchButtonDesc`) allocate on
  every call and are not the eventual HB-5 shared-snapshot shape.**
  `bindings_snapshot()`/`touch_buttons()` return `std::vector<T>` with
  owned `std::string` fields (`bindings.hpp:23-31`, `touch.hpp:88-99`).
  This is explicitly fine today — input-boundary.md's own P-2 row
  records "no off-main consumer yet" and Input is `T_Main`-only, so
  there is no cross-thread read to race — but **no code change is
  proposed here**: HB-5 ("one shared published-snapshot idiom", still
  OPEN tree-wide) needs to decide whether a future off-main
  diagnostics reader (G-1/G-3) gets a copy-under-lock, a
  trivially-copyable-only restriction, or something else for
  string-bearing snapshots specifically, before this subsystem's
  shape can be judged against it. Recorded as input's data point for
  that decision, not as work.

______________________________________________________________________

## Investigated and refuted

- **[F139] Main-thread pinning splits into a by-design SDL-affinity
  core vs. an incidental routing/math core (High, Phase-2 verdict
  REFUTED).** Proposed documenting a BY-DESIGN/INCIDENTAL split in the
  Threading table to prepare for a hypothetical future off-main input
  thread. Contradicts the ratified thread model verbatim
  (`input-boundary.md` "Threading": "input = T_Main, full stop — no
  NetIO/AudioDecoder-style split exists or is planned for this
  subsystem") and duplicates the boundary spec's own P-1 row, which
  already reasons the identical case through to the opposite
  conclusion ("If G-1/G-5 ever inject synthetic events from another
  thread, they marshal through the inbox ... not a bespoke channel").
  Do not re-file.
- **`compliance_scan input`'s `os-socket` "blocker" at `input.cpp:206`
  is a naming-collision false positive.** The flagged line is
  `void Input::bind(Key key, std::string_view command) noexcept` — the
  key-binding method (`bind`/`unbind` console commands), not a socket
  `bind()` call; there is no socket I/O anywhere in `src/input/`. Same
  failure class as the tree-wide `feature_use.naked_new`/
  `requires_clause` regex false-positives recorded in `CORRECTIONS.md`
  — a bare-identifier match with no call-site verification. Do not
  re-file as a real blocker without checking the call site.
- **`compliance_scan input`'s `unique-ptr-nonpimpl` candidate-warning
  at `input.hpp:317`/`input.cpp:48-58` is a false positive.** The
  scanner flags `friend std::unique_ptr<Input> create_input(...)`
  because `Input` is not a pimpl `Impl`, but `Input` *is* a genuine
  pool-owned class per the Q-22/P-7 idiom: `create_input` does
  `memory::mem_alloc` + placement-new (mirroring `pool_new<T>`'s own
  body, per the in-code comment at `input.cpp:48-54`) and `Input`
  carries both `operator delete` overloads (`input.cpp:71-72`, freeing
  via `memory::mem_free`). The `unique_ptr<Input>` return is the
  factory's ownership-transfer wrapper around that pool allocation, not
  a bare `make_unique` bypassing the pool. Do not re-file without
  checking for the paired `operator delete`s.
