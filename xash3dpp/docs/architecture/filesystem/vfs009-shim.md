# VFS009 Compatibility Shim

> **Defined in**: `xash3dpp/include/xash3dpp/private/filesystem/vfs009/vfs009.hpp`,
> `xash3dpp/src/filesystem/vfs009/vfs009.cpp`\
> **Namespace**: `xash::filesystem::vfs009`\
> **CMake option**: `XASH_VFS009_SHIM` (default `ON`)

## Overview

The `VFileSystem009` interface (interface version string `"VFileSystem009"`)
is the C++ vtable exported by Valve's GoldSrc filesystem to mod tools and some
game clients. It is **not** a fixed external ABI contract for the rewrite — game
DLLs are not allowed to link against it directly; they receive it through the
engine at runtime. However, various legacy tools and compatibility layers query
for this interface by name.

The shim is compiled as a single translation unit (`vfs009/vfs009.cpp`) only
when the CMake option `XASH_VFS009_SHIM=ON` (the default). When `OFF`, no
`VFileSystem009`-related code is compiled into `xash3dpp_filesystem`.

## `create_vfs009_interface`

```cpp
namespace xash::filesystem::vfs009 {
    void* create_vfs009_interface(Filesystem& fs);
}
```

Returns a heap-allocated object that implements the Valve `IFileSystem009`
C++ vtable by delegating every method to the supplied `Filesystem` reference.
The return type is `void*` to avoid including the legacy `VFileSystem009.h`
header in any rewrite header.

The caller owns the returned object. To delete it, cast to the concrete type
(or `IBaseInterface*`) and call `delete`.

Returns `nullptr` if compiled without `XASH_VFS009_SHIM` (should never happen
in practice, since the TU is not compiled in that case).

## Delegation model

Each `IFileSystem009` method is mapped to the nearest semantic equivalent on
`xash::filesystem::Filesystem`:

| `IFileSystem009` method | `Filesystem` equivalent | Notes |
|------------------------|------------------------|-------|
| `Mount()` / `Unmount()` | `init()` / `shutdown()` | Lifecycle |
| `AddSearchPath(path, id)` | `add_game_directory(path, …)` | |
| `RemoveSearchPath(path, id)` | `clear_paths()` (approximate) | Legacy API has no per-path remove |
| `FileExists(path)` | `file_exists(path)` | |
| `Open(path, mode, id)` | `open(path, mode)` | `unique_ptr<File>` wrapped in legacy handle |
| `Read / Write / Seek / Tell / Size / Close` | Forwarded to `File` methods | |
| `FindFirst / FindNext / FindClose` | `search(pattern)` | Result iterator state is held in a shim-owned struct |

The shim intentionally keeps the legacy `IFileSystem009` header confined to
`vfs009.cpp`; nothing outside that file needs to know about it.

## CMake configuration

```cmake
option(XASH_VFS009_SHIM "Build IFileSystem009 compatibility shim" ON)

if(XASH_VFS009_SHIM)
    set(XASH_FS_VFS009_SRC vfs009/vfs009.cpp)
    target_compile_definitions(xash3dpp_filesystem PRIVATE XASH_VFS009_SHIM)
endif()
```

When `OFF`, `vfs009.cpp` is excluded from the build and
`create_vfs009_interface` is not defined. Engine startup code should only
call it when the define is present.

## Threading model

The shim delegates all operations to the `Filesystem` reference passed to
`create_vfs009_interface`. Thread safety of those operations is provided by
`Filesystem`'s own locks (`paths_mutex`, `game_mutex`). The shim itself adds
no additional locking.

The `FindFirst` / `FindNext` / `FindClose` iterator state is held in a
per-search-handle struct allocated by the shim. Each search handle must be
used by a single thread at a time; the handles are not shared.

## Error handling

`IFileSystem009` methods that return `bool` or a handle return `false` / null
when the delegated `Filesystem` call fails. No exceptions are thrown.

## See also

- [filesystem-facade.md](./filesystem-facade.md) — `Filesystem` class being wrapped
- [search-path.md](./search-path.md) — `AddSearchPath` / `RemoveSearchPath` semantics
- Legacy source: `filesystem/VFileSystem009.cpp`, `filesystem/VFileSystem009.h`
