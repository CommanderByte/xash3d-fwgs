# Future Filesystem Internals

This folder is reserved for future filesystem C++ implementation files.

The first filesystem pilot should still begin as an adapter behind the existing
`filesystem/` module so behavior can be compared easily. Once that approach is
proven, reusable pieces such as backend interfaces, registry helpers, path
policy helpers, and debug snapshot builders may move here.

Current files:

- `archive_registry.cpp`: archive registry type helpers and default archive
  descriptors; it does not route archive mounting yet
- `debug_snapshot.cpp`: filesystem mount snapshot human and JSON writers
  built on the shared modern debugging utilities
- `directory_backend.cpp`: first private C++ directory backend; legacy
  directory callbacks currently pass through a C++ bridge while delegating to
  the old C behavior
- `registry_snapshot.cpp`: archive registry snapshot capture and human/JSON
  writers for future `fs_registry` output
- `search_path_backend.cpp`: shared backend interface defaults and type-name
  helpers
