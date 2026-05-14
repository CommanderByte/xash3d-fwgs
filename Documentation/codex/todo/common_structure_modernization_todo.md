# Common Structure Modernization TODO

Goal: modernize the repository-root `common/` contracts without breaking SDK,
wire-format, disk-format, renderer, sound, or game DLL compatibility.

Reference plan:
`Documentation/codex/modern/common/structure-modernization-plan.md`.

## Phase 167: Common Header Ownership Audit

- [x] Inventory all root `common/*.h` files by ownership group.
- [x] Identify public ABI, wire/disk layout, internal helper, and platform
  macro headers.
- [x] Record include fanout and high-risk consumers.
- [x] Decide which modern include homes are appropriate before creating code.

Phase 167 evidence:
`Documentation/codex/modern/common/header-ownership-audit.md`.

Phases 168-175 were deferred and renumbered to 760-767 after platform
portability became the next active modernization lane.

## Phase 760: Deferred Common Layout Test Harness

- [ ] Add a `tests/common/` README and first layout-test target.
- [ ] Cover `netadr_t`, `wrect_t`, `con_nprint_t`, and selected entity/event
  records with `sizeof`, alignment, and `offsetof` checks.
- [ ] Add golden checks for packed or byte-order-sensitive helpers.
- [ ] Document which records are layout-frozen.

## Phase 761: Deferred Core Types And Platform Macro Boundary

- [ ] Audit `xash3d_types.h`, `port.h`, `backends.h`, and `defaults.h`.
- [ ] Decide which macros can gain private `constexpr`/enum wrappers.
- [ ] Avoid changing build-selected platform behavior.
- [ ] Add tests for any helper that replaces byte-order or bit helpers.

## Phase 762: Deferred Network Address Pilot

- [ ] Create a private C++ view/helper around `netadr_t`.
- [ ] Preserve the 20-byte packed layout and inline `NET_NetadrType` /
  `NET_NetadrSetType` behavior.
- [ ] Add IPv4, IPv6, undefined, and multicast tests.
- [ ] Route only a narrow non-risky call path if the helper proves useful.

## Phase 763: Deferred Entity And Event Contract Wrappers

- [ ] Audit `const.h`, `entity_state.h`, `event_flags.h`, `event_args.h`,
  `weaponinfo.h`, `pmove.h`, and `q_client.h`.
- [ ] Add typed flag/value helpers for modern internals without removing public
  macros.
- [ ] Add layout tests around `entity_state_t`, `clientdata_t`,
  `local_state_t`, and `usercmd_t`.
- [ ] Avoid touching network delta encoding until golden packet tests exist.

## Phase 764: Deferred Asset And Model Format Contracts

- [ ] Audit `bspfile.h`, `wadfile.h`, `qfont.h`, `com_image.h`, and
  disk-format portions of `com_model.h`.
- [ ] Separate disk-format descriptors from runtime model graph concepts in
  documentation first.
- [ ] Add golden layout/magic/version tests for WAD, BSP, and qfont records.
- [ ] Reuse existing filesystem/model fixtures where possible.

## Phase 765: Deferred Renderer, Client, And Sound API Boundary

- [ ] Audit `render_api.h`, `r_efx.h`, `r_studioint.h`, `triangleapi.h`,
  `ref_params.h`, `cl_entity.h`, `sound_api.h`, `demo_api.h`, `event_api.h`,
  and `net_api.h`.
- [ ] Freeze callback table layout before adding modern service interfaces.
- [ ] Decide what belongs in client/render phases instead of common phases.
- [ ] Add ABI table-order tests where the current surface is used by modules.

## Phase 766: Deferred Include Diet And Modern Homes

- [ ] Reduce unnecessary broad includes found during Phase 167 and the deferred
  common phases.
- [ ] Add private `src/include/engine/common/` headers only for proven wrappers.
- [ ] Keep public `common/*.h` stable and C-compatible.
- [ ] Update documentation when a wrapper becomes a real engine-domain concept.

## Phase 767: Deferred Common Modernization Checkpoint

- [ ] Compare include fanout, test count, and source layout after the pilot
  phases.
- [ ] Decide whether to continue with common contracts or switch to the
  client/render fixture lane recommended by Phase 166.
- [ ] Move completed common TODOs to `done/` where sensible.
- [ ] Run full validation and runtime smoke timing.
