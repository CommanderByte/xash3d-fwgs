# Filesystem Debug Utility Design

## Purpose

Phase 4 defines debugging utilities that make the filesystem understandable
while the implementation moves behind new adapters.

The utilities should answer two questions:

```text
What is mounted?
Why did this path resolve the way it did?
```

They should be useful to humans in the console and useful to tests/tools through
stable JSON output.

## Output Policy

Every utility that emits non-trivial state should support:

- human-readable output by default
- JSON output through a common flag or paired command

Preferred user-facing shape:

```text
fs_path_verbose
fs_path_verbose -json
fs_why maps/c0a0.bsp
fs_why maps/c0a0.bsp -json
```

If command parsing makes flags awkward in the current console, paired commands
are acceptable:

```text
fs_path_verbose_json
fs_why_json maps/c0a0.bsp
```

JSON should include a schema/version field:

```json
{
  "schema": "xash3d.fs.debug.v1",
  "command": "fs_path_verbose",
  "searchPaths": []
}
```

## `FS-DEBUG-001`: `fs_path_verbose`

### Goal

Show the current search path chain with enough context to understand mount
order, writeability, type, and mount reason.

### Human Output

Suggested columns:

```text
#  type     flags                    write  source
0  dir      gamedir                  yes    valve/
1  pak      gamedir                  no     valve/pak0.pak
2  dir      gamerodir,nowrite        no     C:/Steam/Half-Life/valve/
```

Useful details:

- search order index
- backend type
- decoded flags
- writable yes/no
- source path or archive
- mount reason, when available
- parent archive, when a WAD is loaded from a PAK/PK3

### JSON Fields

```json
{
  "schema": "xash3d.fs.debug.v1",
  "command": "fs_path_verbose",
  "searchPaths": [
    {
      "order": 0,
      "type": "directory",
      "source": "valve/",
      "flags": ["gamedir"],
      "writable": true,
      "mountReason": "gamefolder",
      "parentArchive": null
    }
  ]
}
```

## `FS-DEBUG-002`: `fs_why <path>`

### Goal

Explain why a path resolves to a particular file, archive entry, WAD lump, or
miss.

### Human Output

Suggested shape:

```text
query: maps/c0a0.bsp
policy: normal search, gamedironly=false

checked:
  [0] valve/                    miss
  [1] valve/pak0.pak            hit maps/c0a0.bsp

winner:
  type: pak
  source: valve/pak0.pak
  path: maps/c0a0.bsp
```

For rejected paths:

```text
query: ../escape.txt
rejected: parent path segments are blocked while direct paths are disabled
```

### JSON Fields

```json
{
  "schema": "xash3d.fs.debug.v1",
  "command": "fs_why",
  "query": "maps/c0a0.bsp",
  "gamedirOnly": false,
  "directPaths": false,
  "result": "hit",
  "winner": {
    "order": 1,
    "type": "pak",
    "source": "valve/pak0.pak",
    "resolvedPath": "maps/c0a0.bsp"
  },
  "checks": [
    { "order": 0, "source": "valve/", "result": "miss" },
    { "order": 1, "source": "valve/pak0.pak", "result": "hit" }
  ]
}
```

## `FS-DEBUG-003`: `fs_find_all <path>`

### Goal

Show every matching file across search paths, not just the winner.

This is especially useful for diagnosing mod overrides, rodir content, archive
duplicates, and localization/custom folders.

### Human Output

```text
query: sound/common/wpn_select.wav

matches:
  [0] valve_hd/sound/common/wpn_select.wav   directory
  [3] valve/pak0.pak:sound/common/wpn_select.wav pak
```

### JSON Fields

```json
{
  "schema": "xash3d.fs.debug.v1",
  "command": "fs_find_all",
  "query": "sound/common/wpn_select.wav",
  "matches": [
    {
      "order": 0,
      "type": "directory",
      "source": "valve_hd/",
      "resolvedPath": "sound/common/wpn_select.wav"
    }
  ]
}
```

## `FS-DEBUG-004`: `fs_registry`

### Goal

Print the registered archive/backend formats and their metadata.

This should help confirm that a future generic registry is initialized exactly
as the legacy `g_archives[]` table was.

### Human Output

```text
archive registry:
  ext   type    real  auto-wads  priority  backend
  pak   pak     yes   yes        10        PakBackend
  pk3   zip     yes   yes        20        ZipBackend
  pk3dir dir    no    yes        30        DirectoryBackend
  wad   wad     yes   no         40        WadBackend
```

### JSON Fields

```json
{
  "schema": "xash3d.fs.debug.v1",
  "command": "fs_registry",
  "archives": [
    {
      "extension": "pak",
      "type": "pak",
      "realArchive": true,
      "autoMountContainedWads": true,
      "scanPriority": 10,
      "backend": "PakBackend"
    }
  ]
}
```

## `FS-DEBUG-005`: Machine-Readable Format Decision

Decision: support JSON first.

Reasons:

- readable in logs
- easy for tests and agents to parse
- no binary dependency
- no new transport requirement
- stable enough for debug snapshots

Human output and JSON should be generated from the same debug snapshot records.
Do not build human output by scraping JSON strings, and do not build JSON by
parsing human tables.

Suggested snapshot types:

- `SearchPathDebugRecord`
- `PathResolutionDebugRecord`
- `PathCheckDebugRecord`
- `RegistryDebugRecord`
- `ArchiveDebugRecord`

## Trace Mode

Add trace mode later, after the static commands exist.

Potential controls:

```text
fs_trace_find 0/1
fs_trace_mount 0/1
```

Trace mode should be:

- off by default
- unavailable or compiled down in release builds unless explicitly enabled
- bounded, so repeated file lookups do not flood logs forever
- structured internally so `fs_why` can reuse recent trace records later

## Implementation Order

1. Add snapshot structs and human formatters.
2. Implement `fs_path_verbose`.
3. Add JSON formatter for mount snapshots.
4. Implement `fs_registry` once `ArchiveRegistry` exists.
5. Implement `fs_why` with direct calls to backend find hooks.
6. Implement `fs_find_all` after `fs_why`, reusing the same check loop.
7. Add trace mode only after the synchronous commands are stable.

## Testing Expectations

Prefer tests that call snapshot builders directly rather than scraping console
output.

Minimum future tests:

- JSON schema includes expected fields.
- `fs_path_verbose` snapshot order matches `fs_searchpaths`.
- `fs_why` reports hit/miss/rejected path.
- `fs_find_all` includes lower-priority matches.
- `fs_registry` lists PAK, PK3, PK3DIR, and WAD with legacy-compatible
  metadata.

