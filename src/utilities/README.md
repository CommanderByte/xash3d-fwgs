# Utility Implementation Area

This folder is reserved for subsystem-neutral modern utility implementation
files.

Current implementation files:

- `hash.cpp`: legacy-compatible hash-key behavior shared by modern internals and
  C compatibility exports.
- `checksum.cpp`: CRC32 helpers used to pin legacy checksum behavior before any
  broader `crclib` migration.
- `path.cpp`: legacy-compatible path component and extension helpers shared by
  public C compatibility exports.

The first registry helper is header-only because it is a template:
`src/include/utilities/registry.hpp`.
