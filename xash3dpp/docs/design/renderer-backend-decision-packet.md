# Decision packet — the Chunk-13 renderer backend

> **Date**: 2026-07-20 (tree-wide modernization audit, lens L9)\
> **Status**: DECISION OWED — this packet does not decide it, and deliberately
> takes **no position on graphics-API merits**. It records what the tree
> already commits to, what it still owes, and what changes under each option.\
> **Gates**: Chunk 13 (renderer). `implementation-plan.md`: *"Vulkan vs. GL vs.
> multi-backend decision needed before this chunk starts."*\
> **Related**: `thread-model-decision-packet.md` (the adjacent precondition),
> `decisions-architecture.md` Q-6 / Q-10, `threading-model.md` §6

______________________________________________________________________

## 0. Scope — what this audit will and will not say

This is a **structure** audit. It has no standing on driver maturity, mobile
reach, tooling ecosystems, or developer complexity, and it does not rank GL
against Vulkan. What it does have standing on is: which decisions the project
has already ratified, which of this chunk's suppliers have never been
exercised, and what each option makes urgent, moot, or re-tiered.

______________________________________________________________________

## 1. The headline — the render-thread half is already decided

`implementation-plan.md`'s Chunk-12 complexity note demands that "whether
network I/O **or rendering** run off-main" be settled before Chunk 12 starts.
For the rendering half, **three ratified sources already answer it, and all
three put it in Chunk 13**:

| Source | What it says |
|---|---|
| **Q-6 THREADING** (DECIDED) | *"Render thread optional — renderer plugin declares `wants_render_thread`; double-buffered `RenderFrame` is the main↔render boundary."* |
| **Q-10 PLUGIN_VERSION** | *"…for the current plan, settle renderer/backend policy **before Chunk 13** implementation starts."* |
| **`threading-model.md` §3.6** | *"Chunk 13 (renderer) — Model C (optional render thread)."* §6.2 gives the mechanism: `RendererCaps::wants_render_thread` — **false for GL, true for Vulkan** — and the engine starts `T_Render` only if it is true. *"The GL renderer path is permanently single-threaded and that is correct."* |

So the render-thread question is not an open decision at all: it is a
**function of the backend choice**, already specified, and scoped to Chunk 13.
Chunk 12 does not need it. The plan sentence should be corrected to scope its
precondition to networking I/O only — see the thread-model packet §2.

______________________________________________________________________

## 2. What the renderer's suppliers owe — and what has never been exercised

The single most important structural fact for this decision: **`imagelib` has
no in-tree consumer at all.** `ImageDecoder::decode()` has zero production call
sites tree-wide; only tests link it. Its entire output contract — seven codecs,
the `PixelFormat` set, the compressed-format passthrough — **has never been
validated against a real reader.** The renderer is that reader.

Concretely owed at Chunk 13:

1. **Wire `ImageDecoder` into `content::InitParams`** and add the
   content→imagelib `PUBLIC` CMake link. Note the marker is **stale-tagged
   `TODO(Chunk 7)`** in `src/content/CMakeLists.txt` though the plan assigns it
   to Chunk 13 — a stale tag is worse than an untagged TODO, because it hides
   behind a discharged chunk number and is filtered out by any gate scoped to
   open chunks.
2. **Adapt `xash::imagelib::Image` → legacy `rgbdata_t` at the renderer seam**,
   so imagelib itself never freezes on the legacy struct.
3. **Validate the compressed-format passthrough contract** (DXT1/3/5, Ati2,
   BC4-7, `Ktx2Raw`, kept compressed for GPU-side decode) against the chosen
   backend's actual texture-format support. The KTX2 codec's own comment names
   its intended reader — a Vulkan renderer — so this contract was written
   *toward* a backend that has not been chosen.
4. **Implement `content::IModelPostProcess`**, do not add a second hook. It
   already exists, names its own renderer consumer in-header, and its OQ is
   resolved. It is one of the tree's zero-implementation interfaces *for a
   legitimate reason*: it is a scheduled door whose owner is this chunk.

______________________________________________________________________

## 3. Consequence table

| Axis | Under GL | Under Vulkan | Under multi-backend |
|---|---|---|---|
| **`T_Render` spawn** | Moot — `wants_render_thread = false`; the driver owns the submission queue. Chunk 13 can start as a direct Main-thread call (§6.5) | Load-bearing day one; `RenderFrame`'s double buffer stops being scaffolding and becomes required correctness infrastructure | Conditional per plugin — Q-10's versioned descriptor already carries this, but `RenderFrame` must serve a synchronous GL `submit_frame` *and* a threaded Vulkan one from day one |
| **`RenderFrame` (HB-5)** | Optional as the reference shape; can ship late without forcing HB-5's hand | Forced to exist early — becomes the tree's first genuinely shared, load-bearing P-2 idiom, **ahead of** a formal HB-5 brief | As Vulkan, plus the shape must generalise across backends |
| **imagelib compressed formats** | Needs BPTC/S3TC capability checks; desktop GL commonly has both | Desktop: `textureCompressionBC`. **Mobile: ETC2/ASTC — imagelib ships zero such codecs today** | Must satisfy the union; the mobile gap becomes real, not speculative |
| **imagelib `decode()` Main-pin** | Becomes real work once wired | Same, same urgency | Same — **backend-agnostic** |
| **`IModelPostProcess`** | Live, unimplemented | Same | Same — backend-agnostic |
| **`ref_api.h` freedom** | No legacy struct constraint at all; Q-10's descriptor replaces it | Same freedom | Same freedom, but one descriptor shape must abstract two lifecycles — the "most work" option |
| **cmd_cvar retrofit** | Not forced | **Forced** if `T_Render` reads `gl_*`-class cvars per frame — and per the thread-model packet, "add a `shared_mutex`" is *wrong as written* | Forced under the threaded backend |
| **Plain stats structs** | 0 forced atomic | ≥4 forced, and the 13 `blocks-G3-read` findings become urgent together | ≥4 |

______________________________________________________________________

## 4. What can start now, before the decision

Exactly one item is both **backend-agnostic** and **cheap**: make `ImageStats`
atomic, removing the incidental `ThreadRole::Main` pin from
`ImageDecoder::decode()`.

The pin is driven *only* by two plain counters — the codecs are stateless and
const, and pool create/destroy are documented any-thread-safe. `threading-model.md`
§6.4 already specifies the async texture-decode path (a worker decodes pixels
off-Main; Main appends a texture-upload command to the next frame), which needs
`decode()` callable off-Main **identically under either backend**.

The audit's own corpus correctly classified this as a *shape constraint* today,
because `decode()` has no consumer. It flips to real work the moment Chunk 13
names one. Doing it now means Chunk 13 does not rediscover it as day-one debt.

Everything else — `RenderFrame`'s double buffer, any ETC2/ASTC codec — **waits
for the decision**. Building either speculatively has no named consumer, which
is precisely what `extension-goals.md` §6 forbids.

______________________________________________________________________

## 5. Shape constraints (recorded, not scheduled)

- **`RenderFrame`, when built, must be the double-buffered `write_idx`/`read_idx`
  struct already specified in `threading-model.md` §6.3** — atomic index swap at
  the frame boundary, `submit_frame(const RenderFrame &)` called from
  `T_Render` only, never from Main. **Do not invent a second P-2 shape**: it
  becomes the canonical HB-5 reference the moment it exists.
- **`T_Render` is spawned only when the chosen backend's
  `RendererCaps::wants_render_thread` is true.** Never hardcode a render-thread
  spawn independent of the backend's declared preference.
- **imagelib's `PixelFormat` set has no ETC2/ASTC entries.** If the backend
  decision names a mobile-primary target, that gap must close *before* Chunk 13
  can decode mobile-native compressed assets. This is a consequence of the
  desktop-vs-mobile sub-decision, not something to add speculatively.
- **`content::IModelPostProcess` is already the correct injected seam** for
  renderer-side post-load GPU prep. Chunk 13's renderer becomes a production
  implementer of it; no second post-load hook.

______________________________________________________________________

## 6. The decision, stated for its owner

**Q: GL-compat, Vulkan-first, or abstracted multi-backend?**

- **GL-compat** — broadest hardware and driver reach; simplest thread model
  (no `T_Render`, `RenderFrame` optional); nothing in this packet becomes
  urgent.
- **Vulkan-first** — makes `T_Render` and `RenderFrame` load-bearing on day
  one, pulls HB-5 forward ahead of its brief, forces the cmd_cvar question, and
  raises an unresolved ETC2/ASTC question if mobile is in scope.
- **Abstracted multi-backend** — highest effort; one `RenderFrame` and one
  plugin-descriptor shape must serve both a synchronous and a threaded
  submission model, and imagelib must cover the union of both format sets.

**The audit's recommendation is scoped to engine structure only**: Chunk 12
does not need this decision (Q-6 and Q-10 already decouple it); the imagelib
Main-pin fix is safe to do beforehand; and `RenderFrame` plus any ETC2/ASTC
work should wait for the decision and its desktop-vs-mobile sub-choice.

There is an unusual amount of freedom here worth naming: **`ref_api.h` is the
only major interface in the tree that is not frozen.** The constraint is that
GoldSrc *visual output* must match — not that any struct shape must. That is
the strongest argument for not rushing the choice.

______________________________________________________________________

## 7. Re-run triggers

1. The backend decision is made — the conditional rows in §3 resolve.
2. Chunk 12 lands and the plan's thread-model wording is revisited.
3. **imagelib gets its first production consumer.** The "zero consumer, zero
   code" baseline under `RenderFrame`, `ImageDecoder::decode()` and
   `IModelPostProcess` is the load-bearing fact beneath every line above.
