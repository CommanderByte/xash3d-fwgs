# Resource Domain Aggregate Tests

Phase 148 adds an aggregate resource-domain test without changing runtime
ownership. The intent is to prove the extracted resource helpers compose before
we try to shrink adapters or move more `sv_custom.c` / `sv_client.c` glue.

## Call-Site Audit

The resource-domain helpers currently sit behind three legacy server files:

- `sv_custom.c` owns customization propagation, consistency response parsing,
  resource-list mutation, HPAK custom data probes, upload command emission, and
  reliable datagram writes.
- `sv_client.c` owns client download commands, filesystem/HPAK reads, netchan
  fragment scheduling, client resource-list parsing, upload throttling, and
  resource-list cleanup on rejection.
- `sv_game.c` owns game DLL resource callbacks such as model precache, model
  lookup, decal lookup, and force-unmodified registration order.

The modern helpers remain target-neutral. They decide names, descriptors,
download/upload admission, message rows, consistency reservations, hot-resource
announcements, and custom-logo identity, but do not mutate live server state.

## Aggregate Coverage

`tests/engine/resource_domain.cpp` now covers cross-helper flows that individual
tests only covered in isolation:

- Game DLL resource-name normalization feeds resource catalog and download
  decisions.
- `.res` sound-token classification feeds catalog entries, manifest lookups,
  hot-resource path planning, and download matching.
- Consistency reservation data feeds resource-message rows and consistency-list
  bitstream emission.
- Upload estimate and batch decisions feed custom-logo download lookup and
  customization payload writing.
- Custom MD5 identity formatting/parsing is checked inside the upload/download
  path instead of only as a standalone utility.

## Legacy Ownership Guardrails

Phase 148 deliberately keeps these surfaces legacy-owned:

- HPAK lookup and mutation.
- Filesystem reads, writes, model texture probes, and file-size queries.
- Reliable datagram, signon buffer, and netchan message ownership.
- Live `resource_t`, `sv_client_t`, `edict_t`, and `server_static_t` mutation.
- Game DLL callback order and callback table publication.

This means Phase 149 can consider adapter consolidation only for mechanical
plain-value translation. It should not introduce a broad resource facade that
hides the remaining runtime effects.
