# Common Header Ownership Audit

Phase 167 audits the repository-root `common/` folder before any C++
modernization code is added. The goal is to identify which headers are public
compatibility contracts, which are layout-sensitive records, and which are safe
to wrap with private modern helpers.

## Summary

The folder contains 39 headers and should be treated as a compatibility
surface first.

None of the headers should be rewritten in place as C++ classes during the
early common-modernization lane. Most are included by more than one of:

- engine runtime code;
- renderer modules;
- filesystem code;
- public utility code and tools;
- bundled MainUI/VGUI-style code;
- client, render, sound, event, or network callback tables.

Modern C++ work should therefore happen beside the legacy headers:

- public C headers stay in `common/`;
- private views and typed wrappers live under `src/include/engine/common/`
  unless a more specific domain owns them;
- implementation lives under `src/engine/common/` only for genuinely shared
  cross-domain helpers;
- layout-sensitive records get tests before route-through.

## Fanout Snapshot

Direct include counts were measured from `common`, `engine`, `filesystem`,
`public`, `ref`, `utils`, `src`, `tests`, and `3rdparty`.

| Header | Direct includes | Main consumers |
| --- | ---: | --- |
| `backends.h` | 8 | common defaults, engine platform, bundled third-party |
| `beamdef.h` | 3 | render effects |
| `bspfile.h` | 2 | model/file-format headers |
| `cl_entity.h` | 21 | engine client, renderer, bundled UI |
| `com_image.h` | 2 | engine/ref API |
| `com_model.h` | 18 | engine, filesystem, public math, renderer, tools, bundled UI |
| `con_nprint.h` | 5 | engine console/sound, bundled UI |
| `const.h` | 75 | engine, renderer, tools, bundled UI |
| `cvardef.h` | 22 | engine cvar/menu/ref API, renderer, bundled UI |
| `defaults.h` | 4 | engine platform, filesystem |
| `demo_api.h` | 9 | engine client, bundled UI |
| `enginefeatures.h` | 10 | engine, renderer |
| `entity_state.h` | 19 | engine networking/server, bundled UI |
| `entity_types.h` | 20 | engine client, renderer, bundled UI |
| `event_api.h` | 11 | engine common/client, bundled UI |
| `event_args.h` | 6 | common/entity/client network paths |
| `event_flags.h` | 8 | engine server/client, renderer |
| `gameinfo.h` | 5 | engine menu, filesystem, bundled UI |
| `ivoicetweak.h` | 2 | engine client, bundled UI |
| `lightstyle.h` | 2 | render API, engine world |
| `net_api.h` | 4 | engine client/server, bundled UI |
| `netadr.h` | 8 | engine network, net API, bundled UI |
| `pmove.h` | 6 | engine world/server/client, renderer |
| `port.h` | 25 | engine, filesystem, public, ref, src, tests, tools |
| `q_client.h` | 4 | engine client, bundled UI |
| `qfont.h` | 5 | engine console/font/image paths |
| `r_efx.h` | 17 | engine client/ref API, renderer, bundled UI |
| `r_studioint.h` | 10 | engine client/model, renderer, bundled UI |
| `ref_params.h` | 5 | engine client, renderer, bundled UI |
| `render_api.h` | 6 | engine host/client/ref API, renderer |
| `screenfade.h` | 4 | engine client, bundled UI |
| `sound_api.h` | 2 | engine client/sound |
| `studio_event.h` | 3 | engine studio model, bundled UI |
| `synctype.h` | 2 | engine alias/sprite |
| `triangleapi.h` | 19 | engine client/server/ref API, renderer, bundled UI |
| `wadfile.h` | 11 | engine model/image/console, renderer, filesystem, tests |
| `weaponinfo.h` | 5 | entity state, engine network, bundled UI |
| `wrect.h` | 7 | engine/menu UI, VGUI, bundled UI |
| `xash3d_types.h` | 53 | nearly every public/common/engine/ref/src area |

## Ownership Classification

| Header | Ownership | Compatibility risk | Modern home |
| --- | --- | --- | --- |
| `backends.h` | Platform/backend id constants | Build/platform behavior | `src/include/engine/common/core/` typed backend ids later |
| `beamdef.h` | Client/render beam runtime record | Renderer/client ABI | Client/render domain, not generic common first |
| `bspfile.h` | BSP disk-format records and constants | Disk layout and map compatibility | `src/include/engine/common/assets/` descriptors after layout tests |
| `cl_entity.h` | Client entity/render state | Client/render ABI and prediction state | Client/render domain views |
| `com_image.h` | Image format metadata and loaders' shared constants | Engine/ref image pipeline | `assets/` format descriptors |
| `com_model.h` | Mixed model disk/runtime/render graph | Very high; model lifetime, renderer, filesystem, tools | Split later into assets/model/render docs before code |
| `con_nprint.h` | Console notification record | Rendered console ABI | Console/client render view |
| `const.h` | Game DLL/entity/shared gameplay constants | Very high; SDK/game DLL contract | Typed flag wrappers only, likely `protocol/` and server/client domains |
| `cvardef.h` | Cvar public record | Command/cvar ABI | Engine command/cvar domain |
| `defaults.h` | Build/platform defaults | Waf/platform selection | `core/` defaults view only after platform audit |
| `demo_api.h` | Demo callback API table | Client DLL callback table order | Client API boundary tests first |
| `enginefeatures.h` | Engine feature flag constants | Game/render behavior flags | Typed flag wrapper under `core/` or engine config |
| `entity_state.h` | Entity/client/local network state records | Wire/delta/prediction layout | `protocol/` views after layout tests |
| `entity_types.h` | Entity type ids | Renderer/client behavior constants | Typed enum wrapper under `protocol/` or render domain |
| `event_api.h` | Event callback API table | Client DLL callback table order | Client/event API boundary tests first |
| `event_args.h` | Event argument record | Network/event playback layout | `protocol/` value view |
| `event_flags.h` | Event flag constants | Server/client event behavior | Typed flags under `protocol/` |
| `gameinfo.h` | Game metadata/config records | Filesystem/menu/config surface | Engine config or launcher/filesystem boundary later |
| `ivoicetweak.h` | Voice tweak callback API | Client voice interface ABI | Client/audio domain |
| `lightstyle.h` | Lightstyle record | Renderer/world state | Render/model domain |
| `net_api.h` | Network callback API table | Client/network callback table order | Network/client API boundary tests first |
| `netadr.h` | Packed network address record | Wire/storage layout, byte-order helper | `protocol/` pilot candidate |
| `pmove.h` | Movement trace/movevars records | Prediction, server/client physics layout | Movement/world domain after fixtures |
| `port.h` | Platform/compiler compatibility macros | Global platform behavior | Keep C root; add small platform views only when needed |
| `q_client.h` | Usercmd, dynamic light, particle records | Input/prediction/client rendering | Client/protocol domain after layout tests |
| `qfont.h` | Font file/UI record | UI/console asset layout | `assets/` descriptor |
| `r_efx.h` | Effect API and temp entity records | Client/render callback table order | Client/render domain |
| `r_studioint.h` | Studio renderer API | Renderer callback table order | Render domain |
| `ref_params.h` | Render frame/view parameters | Renderer/client ABI | Render domain views |
| `render_api.h` | Renderer interface table and records | Renderer module ABI | Render domain tests first |
| `screenfade.h` | Screen fade record | Client message/render behavior | Client/render protocol view |
| `sound_api.h` | Experimental sound interface table and sound records | Sound engine/client callback ABI | Audio domain, keep C ABI |
| `studio_event.h` | Studio model event record | Model/event layout | Asset/model descriptor |
| `synctype.h` | Sprite sync enum | Model/sprite format behavior | Typed enum wrapper only |
| `triangleapi.h` | Triangle rendering callback table | Renderer/client callback ABI | Render domain tests first |
| `wadfile.h` | WAD disk-format records | Disk layout and filesystem/image behavior | Existing filesystem/assets helpers plus layout tests |
| `weaponinfo.h` | Weapon prediction record | Client prediction/local state layout | `protocol/` view after layout tests |
| `wrect.h` | UI rectangle record | Menu/client/VGUI ABI | Small UI value wrapper after layout test |
| `xash3d_types.h` | Fundamental types, macros, endian helpers | Global C ABI and compiler/platform behavior | Keep C root; mirror selected constants in `core/` |

## Headers That Must Stay C-Compatible

All root `common/*.h` headers should stay C-compatible for now. The strongest
freeze applies to:

- `const.h`, `entity_state.h`, `event_args.h`, `pmove.h`, `q_client.h`,
  `weaponinfo.h`, and `netadr.h` because they touch network, prediction, or
  game DLL behavior;
- `bspfile.h`, `wadfile.h`, `qfont.h`, `studio_event.h`, and disk-format
  portions of `com_model.h` because they describe on-disk data;
- `demo_api.h`, `event_api.h`, `net_api.h`, `render_api.h`, `r_efx.h`,
  `r_studioint.h`, `sound_api.h`, `triangleapi.h`, and `ivoicetweak.h`
  because they are callback/API tables;
- `xash3d_types.h`, `port.h`, `backends.h`, and `defaults.h` because they
  influence platform, compiler, and build behavior.

Even small records like `wrect.h`, `con_nprint.h`, and `screenfade.h` should
keep their public C shape until tests prove any helper is a pure view.

## Modern Include Home Decisions

Use these homes when Phase 168 and later add code:

| Modern home | Use for | Do not use for |
| --- | --- | --- |
| `src/include/engine/common/core/` | fixed-size aliases, typed constants, backend ids, byte-order helper wrappers | public replacement for `xash3d_types.h` or build macros |
| `src/include/engine/common/protocol/` | `netadr_t`, entity/event/usercmd views, typed network/gameplay flags | delta-compression rewrites without golden packet tests |
| `src/include/engine/common/assets/` | BSP/WAD/qfont/image descriptors, validation helpers | runtime model graph ownership |
| `src/include/engine/common/render/` | temporary callback-table views where no narrower domain exists yet | long-term renderer/client implementation ownership |
| `src/include/engine/common/compatibility/` | explicit C adapter declarations | broad facades that hide ABI boundaries |
| `src/include/engine/server/`, `client/`, `models/`, `network/`, or future render/audio domains | helpers once ownership is clearly domain-specific | generic wrappers merely named after old headers |

## Phase 168 Priority

The layout harness should start with:

- `netadr_t`: `sizeof`, alignment, packed offset behavior, IPv4/IPv6 type
  helper behavior;
- `wrect_t` and `con_nprint_t`: tiny UI/console layout sent across old APIs;
- `entity_state_t`, `clientdata_t`, `local_state_t`, and `usercmd_t`: larger
  protocol/prediction records that gate any future entity/event wrapper work;
- one callback table such as `triangleapi_t` or `sound_interface_t` only after
  the basic layout harness is in place.

## Practical Recommendation

Start with `netadr_t` after the layout harness. It is compact, packed,
behaviorally useful, and has a clear current helper pair:
`NET_NetadrType()` and `NET_NetadrSetType()`.

Avoid beginning with `const.h`, `com_model.h`, `render_api.h`, or
`sound_api.h`. They are valuable modernization targets, but they are too broad
for the first common-contract pilot and would blur public ABI, renderer/client
ownership, and internal engine concepts.
