# Private Filesystem Headers

This folder contains private C++ headers for modern filesystem internals.

Current headers:

- `android_assets_backend.hpp`: private C++ Android assets backend and opaque
  runtime callback boundaries
- `archive_registry.hpp`: archive backend descriptor types and archive registry
  typedef over the shared ordered registry utility
- `debug_snapshot.hpp`: immutable filesystem mount snapshot records and writer
  declarations for human and JSON diagnostic output
- `directory_backend.hpp`: private C++ directory backend helpers and runtime
  callback boundaries
- `registry_snapshot.hpp`: immutable archive registry snapshot records and
  writer declarations for future `fs_registry` output
- `search_path_backend.hpp`: internal search path metadata and backend
  interface shaped like legacy `searchpath_t` callbacks

Keep these headers out of public SDK and legacy ABI surfaces. Legacy filesystem
entry points should adapt to these types explicitly when integration begins.
