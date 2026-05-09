# Filesystem Modern Design Decisions

## Purpose

This document records Phase 3 decisions for the filesystem modernization. It is
meant to be prescriptive enough for implementation agents while still leaving
room to learn from the first directory-backend pilot.

The core rule remains:

```text
Modernize inward, preserve outward.
```

The outward surfaces remain `GetFSAPI`, `fs_api_t`, `VFileSystem009`,
`search_t`, `file_t`, current path behavior, and current mount ordering.

## Decision Summary

| Task | Decision | Status |
| --- | --- | --- |
| `FS-DESIGN-001` | Start with a C adapter plus adjacent private C++ implementation, not a wholesale `dir.c` rename. | Accepted |
| `FS-DESIGN-002` | Do not require C++ exceptions or RTTI for filesystem internals yet; define explicit status/error semantics first. | Accepted |
| `FS-DESIGN-003` | Use a narrow internal backend interface shaped like the current `searchpath_t` callback table. | Accepted |
| `FS-DESIGN-004` | Keep `searchpath_t` as the legacy adapter object while C callbacks forward into private C++ backend objects. | Accepted |
| `FS-DESIGN-005` | Build a generic registry primitive, then instantiate an archive registry from it; keep mount-order policy outside the registry. | Accepted |
| `FS-DESIGN-006` | Provide both human-readable debug commands and stable JSON output generated from debug snapshot records. | Accepted |

## Source Layout Decision

Create a reserved top-level `src/` tree for new reusable C++ internals:

```text
src/
  README.md
  include/
    README.md
  filesystem/
    README.md
```

This folder is not a new build root yet. Do not move existing production code
there until Phase 5 has proven the adapter approach against the filesystem
tests. Early implementation may live beside the legacy files when that keeps
diffs smaller, but reusable headers and future cross-subsystem utilities should
prefer `src/include` once build integration exists.

Recommended policy:

- `filesystem/` remains the compatibility module until the pilot proves a safe
  migration path.
- `src/filesystem/` is the eventual home for reusable filesystem internals.
- `src/include/` is the eventual home for private engine-wide C++ utilities.
- Public SDK headers stay in their current public/common locations unless an
  explicit ABI decision says otherwise.

## `FS-DESIGN-001`: Backend Conversion Choice

### Option A: Rename `dir.c` To `dir.cpp`

Advantages:

- Smallest number of files.
- Easy to use C++ directly inside existing functions.
- Minimal adapter ceremony.

Costs:

- Large blame and review churn.
- Easy to accidentally mix behavior changes with language conversion.
- Harder to compare old and new implementation side by side.
- More pressure to convert every static helper at once.
- Can create platform build surprises because the whole file moves from C to
  C++ in one step.

### Option B: Keep C Adapter, Add Adjacent C++ Implementation

Advantages:

- Preserves existing exported/static C entry points while internals move behind
  them.
- Keeps diffs reviewable: adapter changes are separate from implementation
  changes.
- Lets the first patch introduce only one C++ object at a time.
- Lets tests prove behavior before moving more storage.
- Makes rollback easier if the backend object shape is wrong.

Costs:

- Slightly more boilerplate.
- Temporary duplication of concepts during the bridge phase.
- Requires careful ownership comments around adapter-owned objects.

### Decision

Use Option B for the first backend conversion.

The directory pilot should add a private C++ implementation beside the legacy
filesystem module, then have the existing C-style callbacks delegate to it.
Only after the adapter pattern is proven should we consider renaming or moving
larger files.

The initial implementation shape should be boring:

```cpp
namespace fs
{
class DirectoryBackend
{
public:
    int findFile(const char *path, char *fixedName, size_t fixedNameSize);
    file_t *openFile(const char *path, const char *mode, int index);
    int fileTime(const char *path) const;
    void search(stringlist_t *out, const char *pattern, bool caseInsensitive);
    void printInfo(char *dst, size_t size) const;
};
}
```

The legacy callbacks remain the compatibility boundary:

```cpp
static int FS_FindFile_DIR(searchpath_t *search, const char *path,
    char *fixedName, size_t fixedNameSize)
{
    return backendFrom(search)->findFile(path, fixedName, fixedNameSize);
}
```

The first pass may keep the old `dir_t` data tree internally. Replacing the
data structure is a later decision, not part of proving the adapter.

## `FS-DESIGN-002`: Exceptions, RTTI, Errors, And Logging

### Build Reality

The filesystem target currently disables C++ exceptions for non-MSVC compilers
through Waf. That means code that depends on `throw`/`catch` is not portable in
this target today. MSVC may allow exceptions by default, but relying on that
would create different behavior by platform.

### Decision

Do not require exceptions or RTTI in filesystem internals during the pilot.

Use explicit status and result types for new C++ helpers:

```cpp
enum class FsErrorCode
{
    Ok,
    NotFound,
    InvalidPath,
    PermissionDenied,
    CorruptArchive,
    UnsupportedArchive,
    OutOfMemory,
    PlatformError,
    InvariantViolation,
};

enum class FsSeverity
{
    Recoverable,
    Fatal,
};

struct FsStatus
{
    FsErrorCode code;
    FsSeverity severity;
    const char *message;
};
```

Exceptions may be reconsidered later for tightly scoped internal code, but only
if the build policy changes across all supported compilers. Even then:

- never allow exceptions to cross C ABI boundaries
- never allow exceptions to cross engine callback calls
- catch at module boundaries and convert to `FsStatus` or legacy return values
- keep allocation failure and archive corruption recoverable where current code
  recovers
- use fatal errors only where current code uses `Sys_Error` or cannot preserve
  invariants safely

RTTI should not be required. Prefer explicit backend type metadata over
`dynamic_cast` or `typeid`.

### Error Categories

| Category | Examples | Handling Rule |
| --- | --- | --- |
| Recoverable lookup miss | file not found, archive entry missing | Return null or false as today. |
| Recoverable invalid input | rejected path, unsupported archive extension | Return failure and optionally log in developer/debug modes. |
| Recoverable corrupt data | bad PAK/WAD/ZIP fixture, unsupported compression | Refuse mount/load, keep filesystem usable. |
| Platform error | failed open/read/seek, directory scan failure | Return failure; include platform detail in debug snapshot. |
| Fatal invariant | impossible state after initialization, corrupted global ownership | Use current `Sys_Error` behavior or an explicit fatal status converted at boundary. |

### Logging Policy

Introduce a small filesystem logging facade later, but route to existing
`Con_Printf`, `Con_DPrintf`, `Con_Reportf`, and `Sys_Error` until the engine has
a broader logging decision.

Proposed levels:

```text
error, warn, info, debug, trace
```

Proposed categories:

```text
fs.path, fs.mount, fs.archive, fs.registry, fs.gameinfo, fs.policy
```

Performance policy:

- `error`, `warn`, and important `info` logs remain runtime controlled.
- `debug` logs are gated by developer/report settings.
- expensive `trace` events are compiled behind a future `XASH_FS_TRACE` or
  equivalent build option and additionally gated by a cvar.
- debug commands should build snapshots on demand instead of permanently
  storing large traces in release builds.

## `FS-DESIGN-003`: Internal Backend Interface Shape

Use composition plus one narrow polymorphic backend interface. Avoid a giant
"filesystem object" base class.

Recommended concepts:

| Concept | Responsibility |
| --- | --- |
| `SearchPath` | Metadata: filename, type, flags, mount reason, insertion order. |
| `ISearchPathBackend` | Behavior: find/open/search/load/print/close. |
| `FilesystemState` | Eventually owns root paths, search paths, write path, direct-path mode, game list, language. |
| `PathPolicy` | Owns path rejection, direct-path quirks, write-path rules. |
| `GameHierarchyBuilder` | Computes mount requests from gameinfo, basedir, falldir, rodir, and flags. |
| `Registry<T>` | Generic lookup/registration container. |
| `ArchiveRegistry` | Archive-specific registry instance with mount factories and metadata. |

The backend interface should be shaped by current callbacks:

```cpp
class ISearchPathBackend
{
public:
    virtual ~ISearchPathBackend() = default;

    virtual void printInfo(char *dst, size_t size) const = 0;
    virtual file_t *openFile(const char *path, const char *mode, int index) = 0;
    virtual int fileTime(const char *path) const = 0;
    virtual int findFile(const char *path, char *fixedName, size_t len) = 0;
    virtual void search(stringlist_t *list, const char *pattern,
        bool caseInsensitive) = 0;

    virtual byte *loadFile(const char *path, int index,
        fs_offset_t *fileSize, void *(*alloc)(size_t),
        void (*freeFn)(void *))
    {
        return nullptr;
    }
};
```

This is internal-only. Do not put it in public SDK headers.

## `FS-DESIGN-004`: Legacy `searchpath_t` Adapter

The current `searchpath_t` is already a C object with a manual vtable. During
migration it should remain the node stored in `fs_searchpaths`.

Adapter responsibilities:

- allocate the legacy `searchpath_t`
- own or reference the new private backend object
- install C callback shims
- delete backend object from `pfnClose`
- preserve `filename`, `type`, `flags`, and `next`
- preserve existing search path insertion order

Recommended bridge shape:

```cpp
struct SearchPathBridge
{
    fs::SearchPath metadata;
    fs::ISearchPathBackend *backend;
};
```

The exact storage can vary during migration. Options include:

- add an internal-only `void *backend` field to `searchpath_t`
- temporarily store a transition wrapper in the existing backend union field
- keep the old backend struct and let it own a C++ helper internally

Prefer the least invasive option for the first directory pilot. If adding a
`void *backend` field makes the adapter substantially clearer, it is acceptable
because `searchpath_t` is internal, but it still needs a small review note.

Do not replace `fs_searchpaths` with `std::vector` or `std::list` in the first
pilot. The linked list order is compatibility-sensitive and already tested.

## `FS-DESIGN-005`: Generic Registry And Archive Registry

Create a generic registry primitive first, then instantiate archive-specific
behavior from it.

Registry responsibilities:

- register entries by stable key
- look up entries by key
- enumerate entries in stable order
- expose entry metadata for debug output
- reject duplicate keys unless explicitly allowed

Registry non-responsibilities:

- mutating filesystem state
- deciding mount order from game hierarchy
- deciding whether WADs from an archive should be inserted before or after the
  parent archive
- applying path safety policy
- logging noisy per-file lookup traces

Archive registry entry shape:

```cpp
struct ArchiveFormat
{
    const char *extension;
    searchpathtype_t type;
    bool realArchive;
    bool autoMountContainedWads;
    int scanPriority;
    const char *debugName;
    MountFactory mount;
};
```

`scanPriority` records the current archive scan order, but policy owns how that
priority is used. This keeps the registry generic while preserving the current
PAK -> PK3 -> PK3DIR -> WAD scan behavior.

Potential future registry users:

- archive formats
- debug serializers
- path policy rules
- platform service providers
- image/asset loaders if those subsystems later follow the same pattern

## `FS-DESIGN-006`: Debug Output And Serialization

Provide both:

- human-readable commands for engine console users
- machine-readable JSON for tests, tools, and future agents

JSON is the default machine-readable format. Do not add BSON unless there is a
real binary transport or performance requirement. BSON would add complexity and
is not useful in a console-first workflow.

Avoid making every core object serialize itself directly. Prefer snapshot
records:

```cpp
struct SearchPathDebugRecord
{
    int order;
    const char *type;
    const char *filename;
    int flags;
    bool writable;
    const char *mountReason;
};
```

Then serialize snapshots through a debug serializer. This keeps core filesystem
logic independent from JSON formatting and makes unit testing easier.

Formatting policy:

- human output should be compact tables or short explanations
- JSON output should use stable field names and arrays
- include version/schema fields in machine-readable output
- include mount order, type, flags, source path, and writeability
- do not include large trace arrays unless explicitly requested

## Phase 5 Entry Criteria

Before implementation starts:

- Phase 2 tests remain green.
- This document is linked from the task list.
- The debug utility design is linked from the task list.
- `src/` is reserved but not build-integrated.
- The first code patch changes behavior only through an adapter with tests.

