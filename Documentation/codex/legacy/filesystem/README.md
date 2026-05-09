# Legacy Filesystem Documentation

This folder documents the filesystem as it exists today. It is intentionally
descriptive, not aspirational. The goal is to give modernization work a shared
map of current responsibilities, data shapes, call flow, and compatibility
boundaries before we start moving code.

## Documents

- [architecture.md](architecture.md) gives the current module overview,
  object/struct relationships, and key Mermaid sequence diagrams.

## Scope

This folder covers:

- `filesystem_stdio` as a dynamically loaded module.
- The C `fs_api_t` interface exposed through `GetFSAPI`.
- The Valve-style `VFileSystem009` interface exposed through `CreateInterface`.
- Search path ownership and backend callbacks.
- Directory, PAK, WAD, ZIP/PK3, PK3 directory, and Android asset backends.
- Startup, gameinfo discovery, file read, file write, and DLL lookup flows.

This folder should not describe the future modular architecture except where a
note is needed to explain why a legacy behavior matters.
