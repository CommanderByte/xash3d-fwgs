# Utility Implementation Area

This folder is reserved for subsystem-neutral modern utility implementation
files.

Current implementation files:

- `hash.cpp`: legacy-compatible hash-key behavior shared by modern internals and
  C compatibility exports.
- `checksum.cpp`: CRC32 helpers and the shared CRC32 table used by modern code
  and public C compatibility exports.
- `conversion.cpp`: legacy-compatible numeric conversion helpers shared by
  public C compatibility exports and modern tests.
- `path.cpp`: legacy-compatible path component and extension helpers shared by
  public C compatibility exports.

The first registry helper is header-only because it is a template:
`src/include/utilities/registry.hpp`.
