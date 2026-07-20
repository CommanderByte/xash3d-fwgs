# Design brief — HB-6, the introspection channels (P-4)

> **Date**: 2026-07-20 (tree-wide modernization audit, lens L3)\
> **Status**: BRIEF DELIVERED. Discharges HB-6's design-brief gate and names
> its first buildable slice — the `diagnostics_dump` aggregator, **overdue**
> since `debug-stats-design.md` §6.4's trigger (≥3 stats structs) was met.\
> **Related**: `debug-stats-design.md` §6.1/§6.2/§6.4,
> `published-snapshot-brief.md` (HB-5), `extension-goals.md` P-4

______________________________________________________________________

## 1. The sketched aggregator is unbuildable, not merely unscalable

`debug-stats-design.md` §6.2 proposes:

```cpp
void diagnostics_dump( CmdCvarContext &, MemorySubsystem &, ... ) noexcept;
```

At 14 stats structs the positional parameter list is a P-5 problem. But the
harder objection is structural: **`xash3dpp_host` does not link `sound`,
`input`, `content` or `imagelib`.** The positional form therefore demands
**four new link edges into the composition root** — for a debug command —
inverting the layer model to serve diagnostics.

A channel-span form demands **zero**: the root registers only what it already
owns, and the signature never changes as more subsystems are wired.

______________________________________________________________________

## 2. The channel shape

```cpp
// core/diagnostics.hpp
struct DiagSink {
    void ( *write )( std::string_view, void * ) noexcept;
    void *ud;
};

struct DiagChannel {
    const char *name;
    void ( *emit )( const void *self, const DiagSink & ) noexcept;
    const void *self;
};

void diagnostics_dump( std::span<const DiagChannel>, const DiagSink & ) noexcept;
```

Host assembles a fixed `std::array<DiagChannel, N>` from the subsystems it
already owns and registers a `stats` built-in through `cmd_add`, with the sink
writing to console. Each participating subsystem exposes one capture-less
`emit` adapter formatting its own `stats()` into sink lines.

The function-pointer + `void *` row **is the tree's existing callback
convention** — the same shape as the 40+ `cmd_add` sites and `for_each_pool`.
No `std::function`, no RTTI, no exceptions, `alignof <= 8`, identical on x86
and x64. This is not a new abstraction; it is the one already in use.

**Day-one consumer**: a `stats` console built-in, registered exactly like the
existing `cmdlist`/`cvarlist`/`hashstats` commands. It exists in-tree today,
which is what makes this buildable rather than speculative.

______________________________________________________________________

## 3. The real G-3 blocker is not "9 plain structs"

The campaign's headline framing was wrong. Recounted: **14 stats structs — 6
atomic, 7 fully plain, 1 mixed.** But the sharper defect is the **accessor
contract**: six accessors hand back a **live `const &` into plain memory the
owner rewrites**. Four different contracts exist for one concept.

**Three of the six are deletable outright** — subtraction beats atomicization:

| Delete | Why |
|---|---|
| `ClockStats` + `Clock::stats()` + 6 mirror writes in `tick()` | A plain mirror of six atomics that are already published torn-free. **Zero callers** anywhere. It is simultaneously the sharpest offender (a live reference into memory `tick()` rewrites field-by-field) and the least defensible: atomicizing it could never yield the coherent *time tuple* a caller wants. It also carries two false doc claims — the boundary says it returns by value (it does not) and tells a future G-3 author the door is open (it is not). |
| `HostStats` + `Host::stats()` + 4 mirror writes | One enum field exactly duplicating `Host::status()`; zero callers; a live `const &` that contradicts the host boundary's own P-2 row. |
| `CmdCvarStats::peak_cvar_count` / `peak_command_count` | Declared, documented, **never written** — permanently zero, so the aggregator would print two zeros forever. They are also the struct's only non-atomic members. |

**The rule, for the four survivors**: *atomic storage may return `const &`;
plain storage MUST return by value.* Apply to `Content::stats()`,
`Imagelib::stats()`, `Filesystem::stats()`, `StringPool::stats()` — one line
each. `MapLoader::stats()` and `memory::get_stats` are the in-tree precedents.

Be honest about what this buys: by-value does **not** make cross-thread reads
torn-free. It removes the *lifetime* hazard, not the race — and it is the
shape a copying aggregator needs. Write the rule down as well as applying it,
because the save boundary already promises a `SaveStats` that does not exist
and would otherwise be built to the wrong shape.

______________________________________________________________________

## 4. The channels, named — HB-6's actual ask

| Channel | Status |
|---|---|
| **Stats** | 14 structs, uniform `stats()` accessors. The aggregator's substrate. |
| **Cvars/commands** | **Missing as data.** `cvar_describe` describes one cvar you already hold; the only enumerator is `cvar_get_list()`, which returns the **raw ABI linked-list head** — an ABI backdoor of exactly the kind P-4's door rule forbids. `cvarlist`/`cmdlist` sidestep it only by printing straight to console. |
| **Enumerated rows** | Two sanctioned shapes, forced by row type, not taste: a POD row uses `std::size_t snapshot(std::span<Row> out)` bounded by a `limits::` constant (the `PoolStats` precedent); a row owning `std::string` returns `std::vector<Row>` by value and is **cold-path only**. The three string-bearing instances are correctly in the second class — do **not** "unify" them onto the span form. |
| **Logs** | `log_set_callback` is a **single global slot** — a second registrant silently displaces the first. |
| **Entities** | `ServerStats` today. **`EntityView` is NOT a channel** — it is a private-header, zero-cost Q-20 accessor over a *live* edict, unreachable from any frontend and not a snapshot. The plan currently names it as an HB-6 channel; that is a correction owed. |

______________________________________________________________________

## 5. Shape constraints

1. **Atomicizing a plain struct is only valid for INDEPENDENT monotone
   counters.** Correlated multi-field state — e.g. filesystem's
   `game_loaded` + `search_path_count` — must use the HB-5 idiom or be
   declared Main-read-only. Field-wise atomics give race-freedom, **not a
   coherent snapshot**. `ClockStats` is the in-tree proof: six atomic loads
   still cannot yield one consistent time tuple, which is exactly why it is
   deleted rather than atomicized.
2. **A stats struct's thread-safety contract must not vary by build tier.**
   `NetworkingStats`' two plain peaks live only under `XASH_STATS`, so the
   struct is any-thread-read-safe in a release build and racy in a profiling
   build. Tier-2/3 fields must be atomic if any tier-1 field is.
3. **`CvarDesc` is not a snapshot.** It is documented as "a copyable read-only
   snapshot for scripting / UI / autocomplete", but all four string members are
   **borrowed pointers** whose own annotations say "valid until the next
   write". It is Main-immediate-use-only. A G-1/G-5 consumer must copy the
   strings out, or the type must be renamed to stop claiming snapshot
   semantics.
4. **When a second log consumer appears**, replace the single callback slot
   with a small fixed-capacity sink array — register/unregister by fn-ptr
   identity, no heap, no `std::function`. **Never** add a second global slot
   beside the first.
5. **Structured cvar enumeration, when needed**, adds a
   `cvar_snapshot(std::span<CvarDesc> out)`-shaped enumerator. It must never
   walk `cvar_get_list()`. **Do not build it now** — the aggregator needs
   stats, not the cvar list, and `cvarlist` already serves the console.

______________________________________________________________________

## 6. Scope

**In**: the channel types, the aggregator, the `stats` command, the three
deletions, the accessor rule, the four by-value flips.

**Out**: the UDP/TCP external channel (§4 of the stats design) — no consumer.
The cvar enumerator — no consumer. Any atomicity retrofit beyond `ImageStats`,
whose conversion pays for itself independently by converting an incidental
Main pin into a real off-thread decode door for Chunk 13.

______________________________________________________________________

## 7. Re-run trigger

When the stats-struct count changes, when a second log consumer registers, or
when the first G-1/G-3 off-main reader lands. Not a date.
