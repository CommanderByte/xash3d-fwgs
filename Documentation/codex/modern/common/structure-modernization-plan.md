# Common Structure Modernization Plan

This plan covers the repository-root `common/` folder after Phase 166. The
goal is to turn shared C headers into clearer modern C++ internals where that
is safe, while keeping public SDK and binary-layout contracts intact.

## First Constraint

Most files in `common/` are compatibility contracts, not implementation detail.
They are included by engine code, renderers, filesystem code, tools, bundled UI
code, and external-style SDK surfaces. Many records are also sent over the
network, loaded from disk formats, passed through function-pointer callback
tables, or consumed by third-party/game DLL code.

That means the first modernization rule is:

- do not rewrite public `common/*.h` headers into C++ classes in place;
- do introduce private C++ value types, policies, enum wrappers, and views
  under `src/include/engine/common/` or a more specific modern subsystem;
- do preserve exact C ABI, struct layout, packing, constants, and callback
  table order until a later compatibility decision explicitly changes them.

## Current Folder Shape

The folder currently has 39 headers. The rough groups are:

| Group | Headers | Initial stance |
| --- | --- | --- |
| Core types and platform macros | `xash3d_types.h`, `port.h`, `backends.h`, `defaults.h` | Keep as C compatibility roots. Add private `constexpr`, enum, and platform views later. |
| Gameplay/entity/network contracts | `const.h`, `entity_state.h`, `event_flags.h`, `event_args.h`, `weaponinfo.h`, `pmove.h`, `q_client.h`, `netadr.h` | Treat as ABI/wire-layout contracts. Add layout tests before wrappers. |
| File and asset formats | `bspfile.h`, `wadfile.h`, `qfont.h`, `com_image.h`, parts of `com_model.h` | Split disk-format POD from runtime model concepts only after golden layout tests. |
| Runtime model/render data | `com_model.h`, `cl_entity.h`, `ref_params.h`, `lightstyle.h`, `beamdef.h`, `screenfade.h`, `studio_event.h`, `synctype.h` | Keep legacy layout. Modernize as read-only views and typed policies first. |
| Callback/API tables | `demo_api.h`, `event_api.h`, `net_api.h`, `render_api.h`, `r_efx.h`, `r_studioint.h`, `sound_api.h`, `triangleapi.h`, `ivoicetweak.h` | Freeze function-pointer table layout. Wrap behind internal service interfaces later. |
| Small shared records | `wrect.h`, `con_nprint.h`, `gameinfo.h`, `cvardef.h`, `entity_types.h`, `enginefeatures.h` | Good candidates for early typed wrappers and tests, but still public unless proven internal. |

## Target Source Layout

Use this source layout once implementation starts:

```text
src/include/engine/common/
  README.md
  core/               private typed constants, fixed-size aliases, byte-order helpers
  protocol/           entity state, event, user command, net address views
  assets/             BSP/WAD/qfont/image format descriptors and validators
  render/             render/client/sound callback-table views and sink interfaces
  compatibility/      C adapter declarations that convert to/from legacy records

src/engine/common/
  README.md
  core/
  protocol/
  assets/
  render/
  compatibility/
```

Use narrower homes when a type clearly belongs elsewhere. For example, renderer
views may end up under `src/include/engine/client/` or `src/include/engine/render/`
after the client/render audit, and filesystem-only WAD helpers may stay under
`src/include/filesystem/`.

## Migration Method

1. Inventory each header and classify it as public ABI, wire/disk layout,
   internal helper, or platform/build glue.
2. Add layout tests for any structure that crosses a DLL boundary, network
   boundary, save boundary, disk-format boundary, or renderer/client callback
   table.
3. Create private C++ wrappers as value types or views, not replacement public
   headers.
4. Route one narrow call path through an adapter only after both legacy and
   modern behavior are covered.
5. Collapse wrappers into real domain concepts once enough adjacent behavior is
   protected, avoiding another layer of one-file facades.

## Candidate Early Pilots

The safest early pilots are small, layout-sensitive, and easy to test:

- `netadr_t`: a 20-byte packed network-address record with inline type helpers.
  A modern `NetworkAddressView` can make IPv4/IPv6 handling clearer while
  preserving exact storage.
- `wrect_t` and `con_nprint_t`: tiny UI/console records suitable for layout
  tests and typed helper functions.
- `gameinfo_t`/`gameinfo2_t`: useful for configuration-facing modernization,
  but it crosses menu/filesystem/launcher-ish surfaces, so it needs broader
  fixture coverage first.
- `event_flags.h` and `enginefeatures.h`: flag wrappers can reduce raw macro
  usage in modern code without changing the public macro names.

Avoid starting with `const.h`, `com_model.h`, `render_api.h`, or `sound_api.h`.
Those are high-value but broad and should follow after the ownership audit and
layout-test harness exist.

## What Good Looks Like

The end state should not be a C++ copy of every C struct. It should be:

- legacy `common/*.h` remains a stable compatibility surface;
- modern internals use named value objects, views, typed flags, and small
  policy helpers where those improve clarity;
- adapters are narrow and explicit at the C boundary;
- layout-sensitive records have tests for `sizeof`, alignment, selected
  `offsetof` values, and conversion behavior;
- heavily coupled runtime structures are grouped by engine domain rather than
  by historical header location.

## Risks

- Many headers carry Valve/Id-derived compatibility and licensing context.
  Preserve attribution and route any broad header rewrite through the Phase
  1100 licensing audit.
- Some C macros encode platform configuration. Replacing them too early could
  alter Waf configuration, launcher behavior, or renderer/platform selection.
- Public callback tables must not gain C++ types, virtual dispatch, exceptions,
  RTTI, or changed calling conventions at the boundary.
- Packed records like `netadr_t` and file-format structs need exact size and
  byte-order tests before modern code writes them.
