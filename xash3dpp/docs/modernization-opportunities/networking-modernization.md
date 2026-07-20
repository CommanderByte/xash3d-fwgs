# networking Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> C++ standard in use: C++**23** (`xash3dpp/src/networking/CMakeLists.txt`,
> `target_compile_features(xash3dpp_networking PUBLIC cxx_std_23)`; the
> tree-wide `CMAKE_CXX_STANDARD 23`). `std::expected`, `std::span`,
> `std::string_view`, `std::bit_cast`, and `enum class` are used throughout —
> this subsystem was written directly in modern C++, not converted from C.
> Boundary spec: `docs/boundaries/networking-boundary.md`
> Threading: `docs/threading-analysis/networking-threading.md`
> Deep dives: `docs/legacy-survey/deep-dive-networking.md` (transport/netchan/
> buffer core), `docs/legacy-survey/deep-dive-delta-encoder.md` (the codec).
> ABI-/wire-frozen surfaces in this subsystem: the packet-header magic numbers
> (`-1`/`-2`/`-3`), the SPLITPACKET / SPLITPACKETGS framing, the netchan
> reliability scheme, the LZSS header + algorithm parameters
> (`window_size = 4096`, `lookahead = 16`, command-byte bit order), the delta
> field tables + both wire dialects, and the packed 20-byte `netadr_t` at every
> frozen-ABI edge. As a **behavioural** ABI, the produced wire bytes for
> protocol 48/49 traffic are frozen — see the wire-exactness prohibition below.
>
> **Refreshed 2026-07-20** (tree-wide modernization audit, A3-seam pack +
> corpus digest). This pass adds four High and five Medium findings the
> 2026-07-06 as-built pass did not cover: a tooling defect that hides 55
> thread-assert waivers from the compliance scanner (H-1), 11 of those 55
> that are dischargeable today with no new primitive (H-2), the exact
> 8-call-site width of the G-2/NetIO door and why the boundary doc's "open
> by construction" claim is wrong (H-3), and the roughly one-third of the
> subsystem (`PacketPool`, both `LagQueue`s, both `SplitReassembler`s, the
> split-packet and compression paths) that is built, tested, and
> unreachable from production (H-4). It also retires a stale over-claim:
> `IMasterListConfig` was previously read as an orphan with "zero
> production callers"; it is in fact a wired, nullable, documented member
> of the production `NetworkInitParams` (M-4, AMENDED). Existing L-1..L-3
> were independently re-verified (CONFIRMED, unchanged) and are annotated
> in place below — nothing in the 2026-07-06 pass was wrong, it was
> incomplete on the shape/seam axis this pass adds.

## Summary

networking is a large subsystem (24 TUs, 5 layers) but — like `map_loader` — it
was written directly in modern C++23 and is already **highly modern**. There is
no legacy-C residue to convert: the codec is a value-semantic `MessageBuf` over
a caller-owned buffer (no `sizebuf_t` global, no `MSG_*` free-function family),
errors propagate through `std::expected<T, NetError>` (no `Host_Error`
process-kill on the I/O path), addresses are a `NetAddress` value type that
enforces the `ip6_0[0..1] == 0` invariant and writes to a **caller-owned**
`std::span<char>` (the legacy `NET_AdrToString` static return buffer is gone),
the packet-source `net_from` global was eliminated (OQ-8), and both wire
dialects sit behind `IProtocolDriver` / `IDeltaWireFormat` seams selected
per-channel. `compliance_scan.py networking` is **clean**.

The result is that the usual modernization headline — raw casts over wire data,
C string handling, sentinel error returns, static return buffers —
**does not apply**: those idioms were never introduced, or were designed out at
scaffold time. What remains is a short tail of **cosmetic** items plus one
**load-bearing non-opportunity**: the byte-exact wire-format constraint that
forbids "modernizing" any bit codec or delta width in a way that changes the
bytes on the wire (the exact analog of the `map_loader` Q-18 determinism
caution and the `utilities` studio-math caution).

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern that
headlines the utilities / filesystem / cmd_cvar reports is **absent** here — see
the cross-cutting note at the end.

The 2026-07-20 A3-seam pack looked past currency (idiom choice) at **shape**:
what the subsystem exports, what is actually reachable from production, and
whether its own thread-safety documentation is trustworthy. That axis turned
up a different kind of finding than the 2026-07-06 pass — not "this cast is
outdated" but "this 1 MiB pool is permanently resident and nothing acquires
from it", "these 55 waiver comments are invisible to the tool that is
supposed to audit them", and "the door this subsystem exists to open is 8
call sites wide, not 'open by construction' as the boundary doc claims".
None of it touches the wire-exactness fence; all of it is either a deletion,
a wiring-up of an already-existing primitive (`ThreadRole::Main`), or a
documentation correction.

______________________________________________________________________

## The wire-exactness constraint (do NOT "modernize" the bytes)

This is the single most important item and it is a **prohibition**, not an
opportunity. For traffic that identifies as vanilla GoldSrc or Xash3D protocol
48/49, the produced bytes are a **behavioural ABI**: game DLLs, third-party
tools, and real GoldSrc peers parse them. The following files are pinned
byte-/bit-exact to the legacy engine and must **not** be "cleaned up" in any way
that reorders bits, changes a field width, contracts arithmetic, or alters a
sign encoding:

- **The bit codec (`message_buf.cpp`).** The little-endian byte order, the
  bit-packing order in `write_ubit_long` / `read_ubit_long`, the `write_coord`
  1/8-unit fixed-point quantisation, `write_bit_angle`'s `angle * (1<<bits) /
  360` rounding, and the `MSG_ReadStringExt` `'%' → '.'` format-specifier
  defense are all wire-load-bearing. The header comment already pins the
  endianness decision ("swap inline in the implementation, not at call sites"
  if a BE port ever happens). Do not swap the bit order for a "tidier"
  `std::bitset` or reorder the coord scale.
- **The delta field codec (`field_codec.cpp`, `delta_codec.cpp`).** The DT_\*
  field widths, the `SignEncoding` split (GoldSrc sign-magnitude vs Xash two's
  complement — the "GoldSrc broken signed integers" quirk), the ×4000 angle
  premultiply, and the per-field-mark (Xash) vs count+mask (GoldSrc) framing
  are frozen. See `deep-dive-delta-encoder.md` §1–3. A `std::ranges` rewrite of
  the field loop is allowed only if it is proven to emit identical bytes.
- **The split-packet framing (`compat_goldsrc.cpp`, `compat_xash.cpp`).** The
  GoldSrc 4-bit-nibble `packet_id` (max 5 fragments) vs the Xash `short`
  high/low-byte split (up to ~506) are wire discriminators. Do not unify them.
- **The LZSS codec (`compress_lzss.cpp`).** The `LZSS` magic, the 8-byte
  header, `window_size = 4096`, `lookahead = 16`, and the command-byte bit
  ordering are part of the protocol contract (`compress.hpp` says so). The
  `reinterpret_cast<std::uintptr_t>` in the hash bucketing is a masked index
  computation (never dereferenced) and is already SAFETY-annotated — leave it.
- **The OOB / packet-header magic (`oob_packet.cpp`).** The `FF FF FF FF`
  connectionless prefix and the `-2`/`-3` split/compressed discriminators are
  read before netchan is consulted; third-party tools depend on them.

Any future refactor touching these files must re-run the networking wire-format
tests as the acceptance gate, exactly as `map_loader` uses its golden vectors.
The `IProtocolDriver` / `IDeltaWireFormat` seams are the **firewall**: a new or
experimental protocol is a new sibling TU behind the seam, never an edit to the
frozen codecs.

______________________________________________________________________

## Implementation-status table

Every design element the boundary spec calls for is already implemented; there
is no "convert from C" backlog. The table records the modern idioms that are in
place (so a future reader does not regress them) and the short cosmetic tail.

| Design element | Status | Notes |
|----------------|--------|-------|
| Value-semantic `MessageBuf` over caller-owned buffer (no `sizebuf_t` global) | **Implemented** | `std::span<std::byte>`; sticky overflow flag; no exceptions |
| `std::expected<T, NetError>` error propagation | **Implemented** | `Result<T>`; no `Host_Error` kill on the I/O path |
| `NetAddress` value type + invariant, caller-owned `to_string` span | **Implemented** | legacy `NET_AdrToString` static return buffer eliminated |
| `net_from` packet-source global eliminated (OQ-8) | **Implemented** | `from` is a `NetAddress&` out-param — the G-2 NetIO precondition |
| `IProtocolDriver` per-channel wire selection (Q-14) | **Implemented** | GoldSrc (48) + Xash (49) drivers; registry Meyers singleton |
| `IDeltaWireFormat` sibling dialects behind one seam | **Implemented** | Xash mark-bit vs GoldSrc count+mask; `SignEncoding` explicit |
| Link-time compression selection (OQ-7) | **Implemented (LZSS/null)** | `XASH_NET_COMPRESSION`; real **bzip2** deferred (see below) |
| Three-tier `NetworkingStats` (Tier-1 relaxed atomics) | **Implemented** | any-thread read surface; Tier-2/3 compile-gated |
| SAFETY-annotated byte aliasing at wire seams | **Implemented** | every `reinterpret_cast`/`memcpy` carries a SAFETY comment |
| Real bzip2 backend | **Deferred** | `compress_bz2.cpp` TODO until `3rdparty/bzip2` is in CMake |
| Async DNS (`string_to_adr_nb`) | **Deferred** | OQ-4 one-thread model not yet ported (no `dns.cpp`) |
| Netchan pool-allocated `reliable_buf` / fragment queues | **Deferred** | Chunk 7 — currently `vector` stubs (`stub_scan` markers) |

______________________________________________________________________

## High-priority opportunities

### H-1: the "55 thread-assert waivers" figure is a scanner artifact — networking's own boundary doc is a direct casualty

- **File(s)**: `xash3dpp/tools/xtools/checks.py:507-509` (the scanning tool,
  not a networking file — recorded here because networking is where the
  defect's consequences land); `xash3dpp/docs/boundaries/networking-boundary.md`
  (T_NetIO flip table); every `compliance-allow(thread-assert)` comment in
  `src/networking/` (55 sites).
- **Current pattern**: `_scan_thread_assert`'s mutator-definition regex is
  `((?:\w[\w:]*::)?(?:MUTATOR_NAMES))\s*\(` — the matched verb must be
  **immediately followed by `(`**. Of the 30 verbs in `MUTATOR_NAMES`
  (`xtools/rules.py:348-353`), 16 carry no `\w*` suffix wildcard: `init,
  shutdown, reset, flush, clear, add, remove, register, unregister, send,
  transmit, process, connect, disconnect, start, stop`. Any mutator whose
  name *extends* one of those is invisible to the scanner. Measured
  tree-wide: 55 definition sites across 51 distinct names, including
  networking's own `context.cpp:233 send_packet`, `netchan.cpp:594
  transmit_bits`, and `snapshot.cpp:1026 transmit_client` (server-side, the
  door counterpart — see H-3). `networking-boundary.md`'s T_NetIO flip table
  names 7 components but only 4 carry the annotation the flip promises to
  operate on — executing the flip as documented would silently miss
  `get_packet`/`send_packet`, the two functions the whole door exists for.
- **Suggested replacement**: Add the `\w*` suffix wildcard to the 16 bare
  verbs in `MUTATOR_NAMES` (one-line regex fix, owned by tooling, not this
  subsystem), then correct `networking-boundary.md`'s T_NetIO flip table to
  list all annotated components once the scanner can see them. Do **not**
  land the regex fix and the flip-table correction as one commit — the fix
  surfaces ~55 new candidate waiver sites tree-wide that each need
  adjudication (per-site true/false-positive triage), which is separable
  work from the one-line regex change.
- **Boundary-safe**: Yes — tooling and documentation only, zero networking
  source touched.
- **Rationale**: This is a tree-wide tooling defect, but networking is where
  it does the most damage: the subsystem has **zero `assert_thread_role`
  call sites** and depends entirely on waiver comments for its documented
  thread contract (see H-2). A scanner that cannot see half the mutators in
  a waiver-only subsystem means the "55 waivers" headline this campaign's
  own fact base quoted was never a complete inventory, and a future flip
  executed off the boundary doc's table would introduce an unasserted race
  at exactly the two functions (`get_packet`/`send_packet`) the G-2 door
  exists to protect.

### H-2: 11 of the 55 waivers are dischargeable today with no new primitive

- **File(s)**: `src/networking/delta/delta_codec.cpp:83,116,148,199,259,324,531`
  (7 sites); `src/networking/delta/delta_tables.cpp:221,273,327,359` (4
  sites) — all `compliance-allow(thread-assert)` comments deferring to "the
  sim/NetIO flip".
- **Current pattern**: Each of these 11 waivers defers enforcement to an
  undecided future thread-model split, but the role each would assert is
  `ThreadRole::Main` — a role that exists today, is armed at process start
  (`launcher/main.cpp:64`), and is asserted 101 times in `server` alone.
  `DeltaTables`/`delta_codec` are sim-pinned *by design* (game-DLL
  `DeltaEncodeFn` callbacks mutate `DeltaField::inactive` in place
  mid-encode) — the rationale text already says "NOT NetIO" in 11 places.
  The other 44 of the 55 (the transport-stack half: `context.cpp`,
  `netchan.cpp`, `loopback_transport.cpp`, `split_reassembler.cpp`) name
  `ThreadRole::NetIO`, which genuinely has no running thread and cannot be
  discharged yet.
- **Suggested replacement**: Split the waiver population into **blocked**
  (the NetIO-half, genuinely awaiting the thread-model decision) and
  **dischargeable-now** (the 11 sim-half sites above): add
  `assert_thread_role(ThreadRole::Main)` at each of the 11 call sites and
  `register_thread_role(ThreadRole::Main)` in the ~6 role-less delta test
  mains that currently have no role registered. This converts a
  note-to-future-self into an enforced invariant using a primitive that
  already exists — no new construct, no consumer question.
- **Boundary-safe**: Yes — `ThreadRole::Main` and `assert_thread_role` are
  shipped, used 101 times elsewhere; this is wiring, not addition.
- **Rationale**: Networking has zero `assert_thread_role` call sites
  tree-wide today, so there is no runtime backstop if a future change
  accidentally calls delta-encoding code off Main. 11 of the 55 "awaiting a
  decision" waivers are not actually waiting on anything — the decision
  they name (sim vs. NetIO) already resolved *for them specifically* to
  "sim," which is `ThreadRole::Main`. Cost: 11 one-line asserts plus one
  role registration per affected test main.

### H-3: the G-2/NetIO door is exactly 8 production call sites wide, and the boundary doc's "open by construction" claim is wrong

- **File(s)**: `xash3dpp/docs/boundaries/networking-boundary.md:651` (the
  claim); `src/server/clients/snapshot.cpp:1026-1043` (`transmit_client`);
  `src/server/clients/net_io.cpp:105,155` (`read_packets`) — both
  server-side cut sites, cited here because they are the door this
  subsystem's own boundary spec makes a claim about.
- **Current pattern**: `networking-boundary.md:651` states the P-1 door is
  "open by construction." It is not: `transmit_client` and `read_packets`
  each interleave sim-side and NetIO-side work **in one call frame sharing
  a caller-owned stack buffer** — precisely the ownership pattern that
  cannot cross a thread boundary without an owned copy.
- **Suggested replacement**: Not a code change today. Record this as
  **door-debt** in `networking-boundary.md`: name both cut sites, and state
  the precondition explicitly — the datagram must become an *owned* buffer
  routed over the P-1 MPSC inbox, not a span into a caller's stack frame,
  before NetIO can split off Main. This gives HB-4's designed-not-built
  `RunFrame` inbox drain slot its **first named consumer with a file:line**:
  `net_io.cpp:105`.
- **Boundary-safe**: Yes — a documentation correction plus a recorded shape
  constraint; no code changes proposed.
- **Consumer**: HB-4 (P-1 main-thread service inbox — `spawn_thread` +
  `MpscQueue`/`SpscRing` landed, the `RunFrame` drain slot is designed, not
  built) — `[scheduled-chunk-N]`, Chunk 12 (client)'s thread-model
  precondition is the trigger.
- **Rationale**: A boundary spec asserting a door is open when the two call
  sites the door exists for are not decomposed is exactly the kind of false
  claim this campaign found repeatedly (`server-boundary.md:153`,
  `:522`, `:526`, `:528`; `core-boundary.md:230`). Recording the precise
  cut sites now means the Chunk-12 thread-model decision (if it ever
  chooses NetIO-off-main) has a concrete starting point instead of a
  subsystem-wide "just split it" that would silently move `DeltaTables`
  with the transport and race the delta state (see H-2's sim/NetIO split).
  `[EXT:G-2]`

### H-4: roughly a third of networking is built, tested, documented — and unreachable from production; delete the truly dead parts, mark the rest as doors

- **File(s)**: `src/networking/transport/packet_pool.cpp` (whole file, 64 ×
  16384 B = 1 MiB permanently resident per `NetworkContext`, `acquire()` has
  zero non-test callers tree-wide — verified: grep for `.acquire()` /
  `packet_pool` outside the file itself and its own `CMakeLists.txt` returns
  nothing); both `LagQueue`s and both `SplitReassembler`s (split-packet
  path); the compression path (`compress_lzss.cpp`, `compress_bz2.cpp`,
  `compress_null.cpp` beyond the link-time-selected null default);
  `src/networking/master_list.cpp:30` (`k_oob_prefix`, declared, zero
  references); `src/networking/netchan.cpp:855` (`Netchan::bind_stats`, one
  caller — `tests/networking/test_netchan.cpp:1133`); `include/xash3dpp/networking/netchan.hpp:75-79`
  (`NetchanFlags{use_munge,use_bzip2,use_lzss}`, stored at
  `netchan.cpp:213`, read nowhere in `netchan.cpp` — verified) vs.
  `src/server/clients/client_state.cpp:346` (`use_lzss = !is_loopback(from)`,
  written under the belief that it controls wire behaviour).
- **Current pattern**: `PacketPool` is created and destroyed with the
  `NetworkContext` but never acquired from. `NetchanFlags` is copied into
  `Impl::flags` at `netchan.cpp:213` and never read again in the file — the
  one production writer (`client_state.cpp:346`) believes it selects LZSS
  compression; nothing consumes the flag. 6 of `NetworkingStats`'s 10 fields
  have no writer anywhere because `bind_stats`'s only caller is a test.
  `k_oob_prefix` is declared and never referenced.
- **Suggested replacement**: Deletion-first, per subsystem: (a) delete
  `k_oob_prefix` (dead constant); (b) delete `Netchan::bind_stats` and the 6
  `NetworkingStats` fields it exists to populate, once confirmed they have
  no other writer; (c) **mark, do not delete**, the split-packet and
  compression machinery (`PacketPool`, `LagQueue`, `SplitReassembler`) as a
  recorded door — `PacketPool` is the natural vehicle for H-3's owned-
  datagram handoff, so deleting it would just require rebuilding the same
  shape later. Add a boundary-spec row per SUB-5's pattern (lens
  `L11-subtraction`) naming the door and its expected trigger (real
  split-packet/compression traffic, or the H-3 owned-datagram consumer).
- **Boundary-safe**: NeedsVerification on (c) only insofar as *wiring up*
  LZSS/bz2/split-packet touches the HB-2 named kernel (wire bit-codec,
  delta field widths, LZSS parameters) and would change wire bytes — that
  is explicitly **out of scope** for this finding, which proposes marking
  the door, not opening it. (a) and (b) are Yes — pure dead-code removal,
  zero wire-byte impact.
- **Rationale**: Per this campaign's subtraction lens (`L11-subtraction`),
  "prefer deletions" — this is one of the largest deletion-shaped findings
  in the subsystem. But not all of it is dead in the same sense: the
  constant and the stats fields are true zero-consumer dead code; the
  split/compression machinery is a legitimate, tested, but currently-
  unreached feature path (real GoldSrc/Xash peers can send split or
  compressed packets even if this fork's own production paths never
  trigger it locally), so it earns a door label, not a delete. The
  `NetchanFlags` finding is the sharpest: a live production call site
  (`client_state.cpp:346`) writes a field believing it changes wire
  behaviour, and it does not — that is a latent correctness gap dressed as
  a modernization non-issue, worth flagging even though no bytes are wrong
  *today* (loopback traffic, where `use_lzss` currently evaluates false,
  is the only path exercised in tests).

______________________________________________________________________

## Medium-priority opportunities

### M-1: the `PoolHandle` contract is validated at `setup()` and never allocated from

- **File(s)**: `src/networking/netchan.cpp:199-205` (`setup()` rejects a
  missing `PoolHandle` with `"setup: pool handle is required (must be the
  parent NetworkContext's networking pool)"`); `:216` (`impl_->pool =
  config.pool`, stored, never read again in the file); `:31-33` (the honest
  in-code blocker: `"TODO(pool-migration): switch the payload storage onto
  the parent NetworkContext's PoolHandle once a pool-backed byte-vector
  adapter exists"`); `:147-151`, `:220`, `:241` (`Fragbuf::payload` stays
  `std::vector<std::byte>`, `outgoing_fragments` stays
  `std::array<std::deque<FragbufBatch>,2>`, both heap-churn containers
  instead of pool-owned RAII).
- **Current pattern**: `setup()` fails loudly if no pool handle is supplied,
  which reads as "this subsystem is pool-integrated" to anyone auditing
  P-7/Q-22 compliance. In fact every buffer in `Netchan` goes through the
  default heap allocator; the pool handle is stored and never touched
  again. The `TODO(Chunk 7)` comments citing this deferral are stale —
  Chunk 7 is **done**, and this item appears in neither Chunk 7's nor
  Chunk 8's deferred-marker inventory, so it is currently an orphaned
  obligation discoverable only by reading the source.
- **Suggested replacement**: Either (a) wire `reliable_buf` and
  `outgoing_fragments` through `impl_->pool` now that `NetworkContext`'s
  pool exists and is already threaded through `setup()`, or (b) if this is
  intentionally still out of scope, correct the stale `Chunk 7` markers to
  name the actual blocker and owner. The blocker is named honestly in the
  code already: *"once a pool-backed byte-vector adapter exists"* — that
  adapter is a **memory** subsystem primitive, not a networking one, and is
  scheduled by nobody today. Retag the markers accordingly rather than
  leaving them pointing at a chunk that already shipped.
- **Boundary-safe**: Yes — container-lifecycle (vector/deque vs. pool-owned
  RAII) is a memory-ownership shape question; changing the allocator
  backing a buffer does not alter its byte content and touches neither the
  wire codec nor any frozen ABI struct.
- **Rationale**: A `PoolHandle` that is validated-but-unused is
  indistinguishable, from the outside, from one that is load-bearing — a
  reader auditing P-7 compliance would conclude networking is pool-
  integrated and move on. The stale `Chunk 7` markers compound the
  problem: they point at a chunk plan number that already closed, so the
  mechanical marker scan the campaign relies on cannot surface this as
  overdue work. `blast_radius`: 3 call sites in `netchan.cpp` plus the
  `setup()` validation itself.

### M-2: `NetworkContext::stats()` is unguarded, has zero callers, and is cited in four boundary rows as the load-bearing any-thread read surface

- **File(s)**: `src/networking/context.cpp:286-289`.
- **Current pattern**:
  ```cpp
  const NetworkingStats &NetworkContext::stats() const noexcept
  {
      return impl_->stats;
  }
  ```
  Unlike its siblings `protocol_driver()` (`:268-279`, guards `!impl_`) and
  `fragment_pool()` (`:281-284`, ternary-guards `impl_`), `stats()`
  dereferences `impl_` unconditionally — a moved-from `NetworkContext`
  null-derefs on first read. It also has zero callers anywhere in `src/` or
  `tests/`.
- **Suggested replacement**: Add the same `impl_ ? impl_->stats :
  NetworkingStats{}` guard style already used by `fragment_pool()` two lines
  above it, for consistency with the rest of the class's null-safety
  convention. Separately, correct the boundary rows that cite this accessor
  as the load-bearing G-3 any-thread read surface — it is currently
  unexercised, so the claim is aspirational, not demonstrated.
- **Boundary-safe**: Yes — a null-guard addition matching an existing
  in-class convention; no ABI or wire impact.
- **Rationale**: `NetworkingStats` has 4 Tier-1 + 4 Tier-2 atomics but **2
  plain `uint32_t` fields** (`peak_loopback_depth`, `peak_inflight_fragments`,
  gated `#if XASH_STATS`, which no CMake file in the tree currently
  defines). A `const NetworkingStats&` handed to a hypothetical off-Main
  reader is a race by construction on those two fields whenever
  `XASH_STATS` is on — the same latent pattern flagged tree-wide by the
  campaign's stats-tiering lens. Fixing the null-guard is cheap and
  independent of that question; the tier-safety issue should be recorded,
  not fixed here, since it depends on the `XASH_STATS` build-tier decision.

### M-3: `IProtocolDriverRegistry::resolve()` is non-const, forcing a mutable singleton where its siblings use const — extension-bumped to Medium

- **File(s)**: `include/xash3dpp/networking/protocol_driver.hpp:127`
  (`[[nodiscard]] virtual IProtocolDriver *resolve( std::uint16_t protocol )
  noexcept = 0;`); `src/networking/wire/protocol_driver_goldsrc.cpp:121-134,143`;
  `src/networking/delta/wire_format_goldsrc.cpp:117`.
- **Current pattern**: `networking-boundary.md`'s Threading section
  explicitly classifies "const wire-format / protocol-driver singletons" as
  one class: "Safe-RO by construction; immutable after construction;
  readable from any thread." `IDeltaWireFormat`'s two sibling instances are
  `static const` and conform. `IProtocolDriverRegistry` does not — its one
  virtual method is non-`const`, so the backing
  `DefaultProtocolDriverRegistry` singleton must be a mutable `static`
  where a `static const` would otherwise match the documented pattern.
- **Suggested replacement**: Mark `resolve(std::uint16_t) const noexcept`
  in the interface and its one override; change the backing singleton to
  `static const DefaultProtocolDriverRegistry instance;` and
  `default_protocol_driver_registry()`'s return type to `const
  IProtocolDriverRegistry&`. `NetworkContext::Impl::protocol_registry` is
  already a non-owning caller-injected pointer, so the two calling contexts
  need only a `const`-qualifier update.
- **Boundary-safe**: Yes — a type-qualifier tightening on an internal
  interface; changes neither the wire bytes produced/consumed nor any
  frozen legacy struct shape.
- **Rationale**: **Extension bump applied** — this finding is tagged
  `[EXT:G-2]` and promoted from the finder's original Low to Medium because
  it directly affects the "Safe-RO by construction" any-thread-read
  contract the T_NetIO flip depends on: under the flip (H-3), a NetIO
  thread would call `resolve()` to pick the wire dialect per channel, and a
  non-`const` method on a singleton is one accidental mutable-state
  addition away from being an actual race, not just a documentation
  mismatch. `blast_radius`: 24 (per the digest's verified call-site count).

### M-4: master-list satellite is fully built and wired as a nullable injection point, not an orphan — the finding was AMENDED after refutation

- **File(s)**: `include/xash3dpp/networking/networking.hpp:58`
  (`IMasterListConfig *master_list_config = nullptr;` in
  `NetworkInitParams`); `include/xash3dpp/private/networking/context_impl.hpp:26`
  (stored in the live `NetworkContext::Impl`); `src/server/physics/physics.cpp:1801`
  (heartbeat call site, currently tagged `XASH3DPP-STUB(chunk6-S9)`);
  `xash3dpp/docs/implementation-plan.md:188` (`create_master_list_client`
  listed under DEFERRED-with-owner, owner Q-22 memory-integration).
- **Current pattern**: The finder's original claim ("fully built and tested
  but has zero production callers, and no chunk number is pinned to the
  wiring") was **AMENDED** on refutation: `IMasterListConfig` is a wired,
  documented, nullable injection point on a shipped subsystem's init
  surface, and `implementation-plan.md:188` already names an owner. What is
  actually missing is a production **implementation** of
  `IMasterListConfig`, not a caller of the type — a materially weaker gap
  than "zero production callers" implies. Separately, `physics.cpp:1801`'s
  stale `chunk6-S9` marker means the mechanical marker scan cannot see this
  as live work.
- **Suggested replacement**: Not a code change — a scheduling and marker
  correction. (1) Retag `src/server/physics/physics.cpp:1801` from
  `XASH3DPP-STUB(chunk6-S9)` to a live chunk tag: the heartbeat is
  server-side per `server-boundary.md:150/152`, so it belongs with
  dedicated-server wiring, not with the Chunk-12 client. (2) Add a single
  `implementation-plan.md` row naming the owner for the server-side
  `IMasterListConfig` implementation explicitly (today the owner is
  implied by the Q-22 tag on `create_master_list_client`, not stated for
  the config side).
- **Boundary-safe**: Yes — pure scheduling/documentation, zero code change,
  zero fence contact.
- **Rationale**: This is the AMENDED form of the finding, and per this
  audit's verdict discipline the amendment supersedes the original: three
  of the finder's four load-bearing claims (zero callers; no owner in
  `implementation-plan.md`; "orphan type") were false. The one surviving
  residue — a stale chunk marker on the heartbeat call site — is real and
  cheap to fix. `IBaselineResolver` is the subsystem's other genuinely
  zero-production-impl interface (2, not the previously-reported 1); both
  are legitimate scheduled doors per lens `L11-subtraction`'s
  classification, not over-abstraction — label them, do not delete them
  (see L-5).

### M-5: record the sim/NetIO thread-model flip line explicitly — the split runs *through* the subsystem, not around it

- **File(s)**: `xash3dpp/docs/boundaries/networking-boundary.md` (Threading
  section, to be added); cross-reference
  `docs/threading-analysis/networking-threading.md`. Evidence: the 11
  T_NetIO-destined waivers in `context.cpp:81,118`, `netchan.cpp:75,226,253,404,612,813`,
  `transport/loopback_transport.cpp:21,70`, `transport/split_reassembler.cpp:7`
  vs. the 11 sim-destined waivers already discharged by H-2.
- **Current pattern**: The 22 decision-coupled waivers (the entire scope of
  the Chunk-12 thread-model precondition) look uniform from outside — 55
  near-identical `compliance-allow(thread-assert)` comments — but their
  rationale text encodes two different destinations in two different
  phrasings. Nothing in `networking-boundary.md` states this split
  explicitly today.
- **Suggested replacement**: Add a boundary-spec row naming the two halves:
  **transport** (`context.cpp`, `netchan.cpp`, `loopback_transport.cpp`,
  `split_reassembler.cpp` — T_NetIO-destined) versus **delta**
  (`delta_codec.cpp`, `delta_tables.cpp` — sim-destined, explicitly "NOT
  NetIO"). State the two riders: loopback stays on `T_Main` permanently
  (`threading-model.md` §7.2), and the HB-2 fenced wire bit-codec/LZSS/
  delta-width kernels would then execute under two thread topologies, so
  the byte-exact witness must be re-derived per topology if the flip is
  ever taken.
- **Boundary-safe**: TOUCHES HB-2 SCOPE as a documentation rider only — this
  finding does not modify the fenced kernels, it records that a future
  thread-topology change would re-open their byte-exact witness. No code
  change proposed.
- **Rationale**: `[EXT:G-2]`. Anyone reading "move networking off-main" at
  subsystem granularity, rather than at this transport/delta granularity,
  will move `DeltaTables` with the transport and race the delta state that
  is deliberately sim-pinned (see H-2). Networking has zero
  `assert_thread_role` calls tree-wide, so there is no runtime backstop for
  that mistake — the only defense is the documentation being right before
  anyone acts on it. Per lens `L8-thread-model-packet`: this is the entire
  scope of the Chunk-12 NetIO precondition, and it is cheaper to close than
  the "98-waiver debt" headline implies, because only 22 of the tree's
  waivers are decision-coupled and all 22 are here.

______________________________________________________________________

## Low-priority opportunities

All of these are cosmetic — the current code is correct, `noexcept`, and
wire-exact. They are recorded for completeness, not because anything is wrong.

### L-1: `as_chars(std::span<const std::byte>)` view helper — CONFIRMED 2026-07-20 (F82)

- **File(s)**: `src/networking/delta/delta_tables.cpp:233` (delta.lst byte
  buffer re-viewed as `char` for text parsing) and `src/networking/netchan.cpp:752`
  (wire byte buffer re-viewed as `char` for a length-bounded filename field);
  both `reinterpret_cast<const char*>` with SAFETY comments.
- **Modernization**: a one-line `[[nodiscard]] inline std::string_view
  as_chars(std::span<const std::byte>) noexcept` would centralise the well-formed
  byte→char aliasing behind one audited seam. Pure readability; identical to the
  `map_loader` L-2 suggestion — a candidate for a **shared** utilities helper if
  Phase 14 consolidates the pattern tree-wide.
- **2026-07-20 refresh**: re-verified against the current tree. Neither site
  is inside the named wire-codec kernel — `delta_tables.cpp:233` parses
  `delta.lst` script text, `netchan.cpp:721-752`'s `copy_file_fragments`
  extracts a filename from an already-reassembled fragment-stream byte
  buffer, not the live wire bit-codec/LZSS/OOB-magic parse itself. Both
  `reinterpret_cast`s stay byte-identical under an `as_chars()` wrapper
  regardless. `touches_named_kernel: false` confirmed for both sites.

### L-2: `field_codec.cpp` typed `read_field<T>` / `write_field<T>` — CONFIRMED 2026-07-20 (F83)

- **File(s)**: `src/networking/delta/field_codec.cpp:30,37` — `memcpy` in/out of
  a `void* base + offset` at the game-struct ABI offset.
- **Modernization**: the `memcpy` form is the correct, alignment-safe idiom for
  reading a `T` at a byte offset within a foreign struct layout (same reasoning
  as `map_loader`'s `read_record<T>`). `std::bit_cast` does **not** help — the
  source is a slice of a larger object, not a same-sized value. **Verdict: leave
  as memcpy.** Recorded only so a future reader does not "discover" `bit_cast`
  and regress it.
- **2026-07-20 refresh**: re-verified — `field_codec.cpp:27-38` IS the delta
  field-width memcpy kernel (networking's named HB-2 fence).
  `touches_named_kernel: true` is correct, and the "no change" verdict is
  correct: `bit_cast` requires a same-sized source object, and this reads a
  slice of a larger struct at a foreign offset, so `memcpy` is the only
  correct form.

### L-3: `message_buf.cpp` bit-op aliasing helper — CONFIRMED 2026-07-20 (F84)

- **File(s)**: `src/networking/message_buf.cpp:114,147` — two
  `reinterpret_cast<std::uint8_t&>(data_[byte_idx])` for bit read/write.
- **Modernization**: `std::byte`↔`std::uint8_t` aliasing is well-defined; a tiny
  `byte_ref(idx)` inline accessor would DRY the two sites. Cosmetic; the casts
  are already SAFETY-annotated and correct. Must not change the bit order (see
  the wire-exactness prohibition).
- **2026-07-20 refresh**: re-verified — `message_buf.cpp:108-154`
  (`write_one_bit`/`write_ubit_long`) ARE the wire bit-codec kernel.
  `touches_named_kernel: true` is correct. The proposed `byte_ref(idx)`
  accessor wraps the identical `reinterpret_cast<uint8_t&>(data_[idx])`
  expression with zero change to which bits are set/cleared or in what
  order — a mechanical dedup inside the kernel, not a deviation from it.

### L-4: `layer-model.md`'s `net_base` extraction deferral cost estimate is wrong — correct it, do not act on it

- **File(s)**: `xash3dpp/docs/design/layer-model.md:44` (the deferral
  claim); `include/xash3dpp/networking/address.hpp` (`NetAddress` — constexpr
  ctors + `operator==` only); `src/networking/address.cpp` (`from_string` /
  `to_string` / `compare_base` / `mask_compare`, free functions);
  `src/platform/*/os_socket.cpp` (calls none of the above).
- **Current pattern**: `layer-model.md:44` defers extracting `NetAddress`
  into a `net_base` layer because "`NetAddress` carries parsing
  implementation and ~24 networking TUs consume these types in place." That
  cost estimate is verifiably wrong: `NetAddress`'s parsing is **not in the
  type** — `address.hpp` is constexpr constructors plus `operator==`; the
  string-parsing free functions live in `address.cpp`, and `platform/*/os_socket.cpp`
  calls **none** of them. Platform does not link networking today.
- **Suggested replacement**: No extraction now — the owner condition (a
  platform-layer consumer that needs socket address types without pulling
  in networking) is still unmet. Correct `layer-model.md:44`'s cost
  estimate so it stops deterring a future reader with an inflated number:
  the move, when its trigger fires, is a pure header relocation, not a
  parsing-logic untangling.
- **Boundary-safe**: Yes — documentation correction only; no extraction
  performed.
- **Rationale**: `[speculative]` consumer — the shape constraint is
  recorded, not the work. This applies when the first satellite that needs
  sockets without networking lands (a G-5 scripting spike or a G-1 MCP
  transport), per `layer-model.md §2`'s own `TODO(net-base)` marker — not
  before. Recording the corrected cost now means whoever picks this up
  later does not re-derive "24 TUs, tangled parsing" as a reason to avoid
  it when the real number is "one header, zero platform callers."

### L-5: label `IMasterListConfig` / `IBaselineResolver` as recorded doors, not orphans

- **File(s)**: `include/xash3dpp/networking/master_list.hpp:26-65`
  (`IMasterListConfig` — 4 pure virtuals, zero production implementations,
  `create_master_list_client` has zero production callers — all 5 call
  sites are in `tests/networking/test_master_list.cpp`); `IBaselineResolver`
  (declared in the networking public headers, zero production
  implementations — the second of the subsystem's genuinely zero-impl
  interfaces, per the A3 pack's corrected count of 2, not the previously
  reported 1).
- **Current pattern**: Both interfaces read, from the outside, like
  speculative infrastructure — zero prod impls, and (for
  `IMasterListConfig`) a factory with zero production callers.
- **Suggested replacement**: Add a Q-21 "Extension axes" row in
  `networking-boundary.md` for each, per lens `L11-subtraction`'s SUB-5
  recommendation: name the interface, its expected day-one implementer
  (`IMasterListConfig` — a host that configures master-server heartbeats,
  see M-4; `IBaselineResolver` — the Chunk-12 client delta-baseline
  consumer), and the fact that neither has a production implementation
  today. This is documentation, not code.
- **Boundary-safe**: Yes — documentation only.
- **Rationale**: Per the corrected tree-wide accounting (lens
  `L11-subtraction`), only ONE interface in the whole tree (`ICvarObserver`,
  in `cmd_cvar`, not networking) is genuine over-abstraction with no
  chunk owner. `IMasterListConfig` and `IBaselineResolver` are both
  "exercised nullable seams" / "scheduled doors" — legitimate,
  decided-and-built OQ-6/Chunk-12 items awaiting a consumer, not surplus.
  Leaving them unlabeled is what makes the tree's "13 zero-impl interfaces"
  headline misreadable as evidence of over-abstraction when 12 of the 13
  are fine designs that are simply undocumented as doors.

### L-6: `delta_wire_format_for`'s ternary carries a compiler-unchecked exhaustiveness obligation as prose

- **File(s)**: `include/xash3dpp/private/networking/delta/wire_format.hpp:56-63`
  (`delta_wire_format_for`, a 2-way ternary over `DeltaTableSet`, with the
  comment "Extend this switch when a new `DeltaTableSet` value is
  introduced"); `xash3dpp/CMakeLists.txt:16` (MSVC branch sets `/EHs-c-
  /GR- /fp:precise`, no `/W4`, no `/w14062`).
- **Current pattern**: The comment promises "compiler-checked exhaustiveness"
  in spirit but the code is a ternary, which cannot be checked by the
  compiler at all, and the build sets no `/w14062` (MSVC's level-4,
  off-at-`/W3` warning for a `switch` missing an enumerator), so even a
  `switch` here would not currently be enforced.
- **Suggested replacement**: Add `/w14062` to the MSVC branch of
  `xash3dpp/CMakeLists.txt:16` (GCC/Clang already emit `-Wswitch` by
  default for enums without a `default:` label — this is an MSVC-only
  gap). Convert `delta_wire_format_for` from the 2-way ternary to a
  `switch` over `DeltaTableSet` with no `default:` label, and delete the
  prose comment obligation — the compiler now owns it.
- **Boundary-safe**: Yes, with one caution. `/w14062` is diagnostic-only
  and cannot alter codegen, so no HB-2 kernel output can shift. The
  `delta_wire_format_for` edit selects between the same two existing
  format-object singletons and emits no bytes itself — but it sits
  adjacent to the networking wire bit-codec fence, so the change must be a
  pure control-flow rewrite with no reordering of the format objects' own
  logic.
- **Rationale**: Per lens `L2-registry-unification`'s L2-R5 recommendation
  (effort S, tree-wide gate bounded by 5 defaultless switches out of 57
  total). This converts a documented-but-unenforced obligation into an
  actual build gate for every closed-set dispatch that adopts the same
  pattern, at negligible cost. `blast_radius`: 1 file, 1 CMake line.

______________________________________________________________________

## Out of scope / ABI-frozen

Beyond the wire-exactness constraint above (the primary out-of-scope
surface for this subsystem), the following are deliberate non-opportunities
that a future modernization pass should not re-propose:

- **Do not replace the `IProtocolDriver` / `IDeltaWireFormat` virtual seams
  with templates.** They are runtime-selected per `netchan_t` (a server speaks
  GoldSrc to one client and Xash to another concurrently), so the polymorphism
  is load-bearing, not incidental. `/GR-` (no RTTI) is respected — these are
  plain virtual dispatch, no `dynamic_cast`.
- **Do not fold the `compress_null.cpp` / `compress_bz2.cpp` link-time pair into
  a runtime `if`.** The OQ-7 link-time selection keeps zero `#ifdef` in the core
  and lets dedicated builds drop the bzip2 dependency entirely.
- **Do not add a networking RNG.** `qport` is caller-supplied
  (`NetchanConfig::qport`) and `LagQueue` takes the caller's drop decision by
  design — keeping randomness out of the subsystem is what makes it
  deterministic and testable. The legacy `Netchan_Init` random-qport global was
  removed on purpose. Networking is HB-12-clean (the shared-deterministic-RNG
  backlog item) and constrains that brief not at all.
- **Do not "modernize" the endianness.** All primitives are little-endian on the
  wire; the header comment pins the decision. A BE port swaps inline in the
  codec, never at call sites.
- **Do not wire up LZSS/bz2/split-packet reachability speculatively** (see
  H-4) — doing so would touch the HB-2 named kernel and change wire bytes;
  mark the door, do not open it without a real consumer.
- **Do not add a second pool-iteration or snapshot API** to
  `NetworkContext` — the tree's demonstrated failure mode (per lens
  `L11-subtraction`) is adding a second API shape to serve a capability
  whose first shape has zero production consumers. If `NetworkContext::stats()`
  (M-2) ever gains a real off-Main consumer, extend it; do not duplicate it.

______________________________________________________________________

## `strnicmp` / `strncmp` string_view over-read — ABSENT

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern
(utilities M-4 / filesystem M-7 / cmd_cvar M-5) is **absent** in networking.
The only string comparisons in the subsystem are `::xash::utilities::strcmp`
(full C-string, NUL-terminated) over **delta field names** — `field.name` /
`func_name` are fixed `char[]` arrays or NUL-terminated literals from
`delta.lst`, guarded with `!name || !name[0]` before use (see
`delta_tables.cpp`, `lst_parser.cpp`, `table_wire.cpp`, `field_codec.cpp`).
There is no bounded-length compare fed a non-NUL-terminated `string_view`.

The candidate the cross-cutting note flags — **info-string / userinfo key
parsing** (`Info_ValueForKey` / `net_api_t::ValueForKey`) — does **not exist in
the shipped code**: the `net_api_t` info-string helper family is deferred (it is
client-DLL-facing and arrives with the client subsystem), so no over-read site
exists here today. When those helpers are ported, they should be written against
the bounded `ci_compare(sv, sv)` the sweep proposes, not the legacy
`Info_ValueForKey` static-buffer form. This is networking's **7th consecutive
negative** data point after platform / core / host / abi / launcher / map_loader
— the pattern is confined to the four early text-heavy subsystems.

______________________________________________________________________

## Open questions

- **Does the Chunk-12 thread-model decision move NetIO off-main, and if so,
  when?** (H-2, H-3, M-5). The binding default per `threading-model.md`
  §3.5/§3.7 is "stay Main-only through Chunk 12," triggered only by one of
  two named events (an async DNS/HTTP consumer, or a 64+-player load
  target) — neither has fired. If this campaign's Chunk-12 planning
  ratifies that default, H-2's 11 dischargeable waivers should still land
  now (they cost nothing and are correct either way); H-3's door-debt
  record and M-5's boundary-spec row should land regardless of the answer,
  since both are pure documentation.
- **Who implements `IMasterListConfig` for a real master-server host?**
  (M-4, L-5). The injection point and the client factory are built and
  tested; the production implementation is not scheduled by name anywhere
  beyond the Q-22 tag on `create_master_list_client`. This needs an owner
  before or during whichever chunk first stands up a connectable
  listen/dedicated server path — server-browser visibility is meaningless
  before that exists.
- **Who owns the "pool-backed byte-vector adapter" M-1 names as the real
  blocker for netchan's pool migration?** It is a `memory`-subsystem
  primitive, not a networking one, and is scheduled by nobody today. This
  needs either an owner (likely a memory-subsystem HB item) or an explicit
  decision to leave `reliable_buf`/`outgoing_fragments` on the heap
  indefinitely and say so in the boundary doc instead of citing a closed
  chunk number.
- **Does the tree-wide `MUTATOR_NAMES` regex fix (H-1) land as one commit
  with adjudication of the ~55 newly-surfaced candidate sites, or split
  into a mechanical regex fix followed by a separate adjudication pass?**
  This campaign's own guidance is to land them as separate commits; the
  adjudication itself is out of scope for this report since most of the
  newly-surfaced sites are outside networking.
