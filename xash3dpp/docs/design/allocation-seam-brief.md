# Design brief — HB-7, the aligned-allocation door (P-7 / G-5)

> **Date**: 2026-07-20 (tree-wide modernization audit, lens L6)\
> **Status**: **REMAINS OPEN AND UN-BUILT — correctly.** This brief discharges
> HB-7's design-brief gate by establishing the door's shape, and **corrects the
> backlog entry's own premise**: the alignment lift is *not* a G-5
> precondition.\
> **Related**: `scripting-runtime-brief.md` §5, `published-snapshot-brief.md`
> (HB-5, a second requirer), `extension-goals.md` §6

______________________________________________________________________

## 1. The correction that matters most

HB-7's backlog text implies the `alignof(T) <= 8` ceiling is a blocker for
G-5's allocator-hook requirement. **Traced against the three shortlisted
runtimes, it is not.**

| Runtime | Allocator hook | Per-VM attribution | Needs the alignment lift? |
|---|---|---|---|
| **Lua 5.4** | `lua_Alloc(ud, ptr, osize, nsize)` — realloc-shaped, size-only | **Yes** — `ud` can box a `PoolHandle` routed straight into `mem_alloc`/`mem_realloc`/`mem_free` | **No.** `TValue`/`lua_State` need at most 8-byte natural alignment on both MSVC targets |
| **QuickJS-ng** | `JSMallocFunctions` — size-based | **Yes** — per-VM opaque context | **No.** No alignment caveat anywhere in the runtime brief |
| **AngelScript** | `asSetGlobalMemoryFunctions` — **global and context-less** | **No** — no per-VM handle to carry a `PoolHandle` | **No.** Its gap is routing, not alignment |

The runtime brief's "complete allocator hook" constraint actually conflates
**two separable questions**:

- **(a) per-VM attribution shape** — Lua and QuickJS clear it natively;
  AngelScript needs a TLS-routing or pool-level-accounting shim. This is the
  real differentiator the spike should score.
- **(b) payload alignment** — **all three clear it today**, at ≤8 bytes, with
  zero HB-7 dependency.

**Nobody should let HB-7 sit on the critical path of the G-5 runtime pick or
its spike.** That correction is this brief's primary deliverable.

______________________________________________________________________

## 2. The door's shape, when it is eventually opened

`AllocHeader` is a fixed 8 bytes — `pool_index` + `payload_size` — prepended
immediately before the payload with **no back-offset field**. `mem_free`'s
`header_of(ptr)` is a fixed, unconditional `static_cast<AllocHeader *>(ptr) - 1`.
All **11** real `pool_new<T>` sites depend on that fixed offset, and none is
over-aligned today.

**Do not grow the shared header.** That would add a back-offset field to
*every* allocation in the tree — hundreds of ≤8-byte-aligned sites — to serve
zero current callers. That is the same over-abstraction the door rules forbid,
just paid in bytes rather than interfaces.

The correct shape is a **second, additive path**:

- `mem_alloc_aligned(pool, size, align)` over-allocates, shifts the returned
  pointer forward to satisfy `align`, and writes a back-offset (realistically
  a ~16-byte header after padding) so a paired free can walk back to the real
  allocation.
- **The free side must be a distinct entry point** — `mem_free_aligned` /
  `pool_delete_aligned<T>`. `mem_free`'s fixed `ptr - 1` arithmetic cannot
  safely disambiguate an aligned block from a normal one without a tag, so
  overloading it is not an option. This mirrors the P-7 dual-`operator delete`
  idiom the tree already uses.
- `architecture/memory/typed-helpers.md` already documents the
  operator-delete-selection half (an over-aligned `T` triggers the compiler's
  `operator delete(void *, size_t, std::align_val_t)` overload). It does **not**
  document the header/back-offset half — that is the piece derived here.

**Net: fully additive.** One new header shape, two new free functions, one new
template. The existing 8-byte path and its 11 callers are untouched.

______________________________________________________________________

## 3. Requirers

| Requirer | Status |
|---|---|
| **G-5 scripting runtimes** | **Not a requirer.** See §1. |
| **HB-5's publish primitive** | A **real** future requirer: a double buffer wanting `alignas(64)` false-sharing separation between its two slots cannot be `pool_new`'d. Either the primitive stays 8-byte-alignable or HB-7 lands first. Speculative today — HB-5 is itself gated — but more concrete than the G-5 claim it replaces. |
| `AllocStrategy::Arena` / `::Slab` | Reserved, not built. When implemented, the strategy selection becomes the `IAllocatorBackend` seam HB-7 names. Keep it a keyed table of function-pointer triples, not an if-chain. |

______________________________________________________________________

## 4. The seam is already singular — the gap is adoption, not shape

Across all 12 subsystems that touch memory there is **one uniformly-shaped
seam**: `create_pool`/`destroy_pool`, `mem_alloc`/`mem_calloc`/`mem_realloc`/
`mem_free`, `pool_new<T>`/`pool_delete<T>`, `PoolHandle`. No competing
allocation primitive exists anywhere beyond two already-waived exceptions (the
sanctioned pimpl `make_unique`, and one annotated `new(std::nothrow)`).

So **an `IAllocatorBackend` would solve nothing that exists today**, and
bridging Lua or QuickJS needs only the existing free functions called from
inside the isolated satellite's `noexcept` shims — no new abstraction layer.
It stays correctly deferred behind the "no `I*` until a second real backend
lands" rule.

**The one real allocation defect the sweep found is adoption**: `content`,
`imagelib` and `map_loader` each **create and destroy a pool with zero
allocations routed through it**. Three dead pools. That is owned by those
subsystems' reports, not by HB-7 — recorded here so the two problems are not
conflated again.

______________________________________________________________________

## 5. Verdict

**HB-7 stays open and un-built.** A Phase-1 finding proposing the lift as
*work* was refuted by the gold-plating critic: its consumer was self-declared
speculative, the constraint is deliberate and self-documenting at the
`static_assert`, and all 11 instantiation sites are under-aligned.

What changes is the backlog text: strike the implication that HB-7 gates G-5,
name HB-5 as the more concrete future requirer, and record the additive
two-path shape above so that whoever eventually opens the door does not reach
for the shared header.

______________________________________________________________________

## 6. Re-run trigger

When a genuinely over-aligned type needs pool ownership — realistically HB-5's
cacheline-padded double buffer, or an SIMD-aligned buffer in the renderer.
Not on the G-5 spike, which this brief removes as a trigger.
