# filesystem/

## Purpose
Virtual filesystem abstraction layer for game asset I/O. Multiplexes multiple archive backends (PAK, WAD, ZIP, directories, Android assets) with configurable search paths and GoldSrc compatibility semantics (case-insensitive lookup, embedded resource indexing). Loaded as a plugin by the engine; both C and C++ interfaces provided.

## Source Files

- **Core**: `filesystem.c`, `filesystem.h`, `filesystem_internal.h` — search path management, file lifecycle, caching
- **Backends**: `pak.c`, `wad.c`, `zip.c`, `dir.c`, `android.c`
- **C++ adapter**: `VFileSystem009.cpp`, `VFileSystem009.h` — GoldSrc binary interface compliance
- **Export**: `exports.txt` — `GetFSAPI` plugin entry point

## Key Data Structures

- `searchpath_t` — archive descriptor with union of backend pointers and function-pointer vtable (`pfnOpen`, `pfnSearch`, `pfnLoadFile`, …)
- `file_t` — file handle with compression state, buffering, position, pack offset
- `pack_t` / `zip_t` / `wfile_t` / `dir_t` — backend-specific archive metadata
- `ztoolkit_t` — zlib decompression stream

## Public Surface (Plugin ABI)

Entry: `GetFSAPI` returns an `fs_api_t` struct of ~40 function pointers. Core operations: `InitStdio`, `AddGameDirectory`, `Search` (pattern matching), `Open`/`Read`/`Write`/`Seek`, `LoadFile` (whole-file cache), path manipulation, DLL lookup. C++ wrapper `IFileSystem` (`VFileSystem009` v-table) provides GoldSrc compatibility.

**This ABI is internal — the rewrite is free to redesign it.**

## Dependencies

- **public/**: crtlib, crclib, miniz, utflib, library_suffix
- **common/**: com_strings.h, xash3d_types.h, gameinfo.h, protocol.h
- **Does not depend on engine/** — properly isolated via function pointers for memory allocation and console printing (injected at initialization)

## Coupling and Risks

- **Backend plug-in via function pointers** — `searchpath_t` union and vtable tightly couple core logic to backend internals; changes cascade
- **Case-insensitivity emulation** — `dir.c` emulates case-insensitive lookup on POSIX with precomputed hash tables; complex and a perf cliff at scale
- **Search path ordering and shadowing** — first-match semantics mask conflicts; no canonical path resolution or audit trail
- **GoldSrc resource indexing quirks** — WADs nested in archives, BSP lump textures, HD pack handling — implicit contracts scattered throughout
- **Dual API surface** — `VFileSystem009` C++ layer must mirror C API exactly; maintenance burden

## Modernization Opportunities

This is the most isolated subsystem and the obvious **pilot candidate** for the rewrite.

- **Type-safe backend registry** — replace function-pointer union with `std::variant<Dir, Pak, Zip, Wad, Android>` or polymorphic base; eliminate error-prone indexing
- **Lazy archive TOC loading** — pre-scan headers at mount but defer decompression; reduce startup I/O
- **Async streaming API** — callback-based read predicates and prefetch hints for video/audio
- **Modern compression backends** — zstd/lz4 alongside deflate
- **Declarative search path manifests** — structured config (TOML/JSON) replacing 27+ flag bits; reduce shadowing bugs
