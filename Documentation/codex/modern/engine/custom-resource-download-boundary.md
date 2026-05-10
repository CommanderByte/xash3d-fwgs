# Custom Resource And Download Boundary

Phase 61 keeps the server resource/download migration in audit mode. The target
architecture should make the policy testable while keeping compatibility-heavy
effects in the legacy adapter.

## Ownership Split

| Layer | Owns | Must not own yet |
| --- | --- | --- |
| Modern resource identity helpers | `!MD5` name parsing, MD5 formatting inputs, resource type/flag snapshots, sound-name comparison, size summaries. | `resource_t` ABI layout, HPAK file access, `MSG_*`, `Netchan_*`, game DLL callbacks. |
| Modern download policy helper | Allow/fail/send decisions from plain inputs and a resource catalog snapshot. | Opening files, checking sidecar existence directly, creating fragments, sending `svc_filetxferfailed`. |
| Legacy server adapter | Cvar reads, `sv.resources[]` snapshots, filesystem/HPAK probes, netchan sends, list mutation, memory ownership. | New policy rules not covered by modern tests. |
| Legacy C surface | `SV_*`, `COM_*`, `HPAK_*`, `resource_t`, `customization_t`. | C++ exceptions or new public ABI types. |

## Phase 62 Target: Resource Identity

Start with pure helpers that do not need live server state:

- parse and validate uppercase `!MD5` names exactly as
  `COM_IsSafeFileToDownload()` allows them;
- format a custom logo request name from a 16-byte hash;
- compare regular download names against resource names, including the legacy
  sound rule where a request for `sound/foo.wav` matches a `t_sound` resource
  named `foo.wav` by advancing past the first six characters of the requested
  path;
- summarize resource-list sizes by type, preserving the `t_model`/`nIndex == 1`
  to `t_world` reporting rule;
- expose resource type names for diagnostics without changing existing
  user-facing strings yet.

Suggested tests:

- uppercase `!MD5` valid, lowercase hex invalid, bad length invalid, extension
  invalid;
- regular unsafe filenames rejected through current safe-name behavior;
- `sound/foo.wav` matches a sound resource named `foo.wav`;
- a sound request without the `sound/` prefix preserves the legacy blind-skip
  behavior;
- a regular model request must match the stored model name directly;
- world model size is counted under `t_world`.

Smallest safe route-through:

- use the identity helper from the server download policy adapter or from
  `COM_SizeofResourceList()` only after the helper's behavior matches the
  baseline. Do not alter HPAK or netchan behavior in Phase 62.

## Phase 63 Target: Download Policy

After identity helpers exist, create a policy result type for
`SV_DownloadFile_f()`:

```text
Reject(name)
SendFile(name)
SendFileWithModelTexture(name, textureName)
LookupCustomLogo(hash)
Ignore
```

The helper should consume:

- requested name;
- `allowDownload`;
- `sendResources`;
- `sendLogos`;
- a snapshot of precached resources;
- optional model texture sidecar availability supplied by the adapter.

The helper should not call `FS_FileExists()`, `Mod_StudioTexName()`,
`HPAK_ResourceForHash()`, `HPAK_GetDataPointer()`, `Netchan_*`, or
`SV_FailDownload()`.

Important compatibility detail: a syntactically valid `!MD5` custom-logo
request can currently do nothing when HPAK lookup fails. Model that explicitly
as `LookupCustomLogo` followed by an adapter-owned no-op on missing data, unless
we deliberately choose to change behavior in a later compatibility phase.

Phase 63 implementation note: this policy now lives in
`src/engine/server/server_download_policy.cpp`, with the legacy bridge in
`engine/server/server_download_policy_adapter.cpp`. The adapter supplies cvar
values and `sv.resources[]`; `sv_client.c` asks the helper whether a model
texture sidecar probe is needed before calling `FS_FileExists()`. Legacy code
still owns `FS_*`, HPAK, fail messages, and `Netchan_*` calls.

## Later Phase Candidates

### Phase 64: Client Resource Upload Queue Helper

Extract target-neutral decisions from `SV_ParseResourceList()`,
`SV_EstimateNeededResources()`, `SV_BatchUploadRequest()`, and `SV_CheckFile()`.

Keep message reads, node allocation, HPAK lookups, upload command writes, and
`sv_client_t` mutation in legacy code. The helper should answer:

- is the client resource descriptor valid?
- should this resource move to on-hand immediately?
- should the server request an upload for this custom decal?
- does the upload total exceed the configured limit?

### Phase 65: Resource Message Serialization Helper

Baseline and then isolate the stable `SV_SendResource()` resource-row encoding.
This can reuse existing modern network-buffer primitives, but the adapter should
continue to own `MSG_BeginServerCmd()` and `Netchan_CreateFragments()`.

### Phase 66: Consistency Resource Policy

Audit and extract the pure parts of `SV_TransferConsistencyInfo()` and
`SV_ParseConsistencyResponse()`. This must wait until tests cover:

- exact-file MD5 comparison;
- model same-bounds and specified-bounds response rules;
- invalid force type handling;
- game DLL `pfnInconsistentFile()` ownership.

## Acceptance Standard

For each follow-up phase:

1. document the legacy behavior first;
2. add modern tests for compatibility edge cases;
3. keep `resource_t` and `customization_t` as adapter-facing legacy data;
4. preserve all cvar names and default behavior;
5. run focused tests, `.\waf.bat build --alltests`, and a runtime smoke if
   legacy routing changes.
