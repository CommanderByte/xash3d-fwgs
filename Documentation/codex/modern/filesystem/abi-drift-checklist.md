# Filesystem ABI Drift Checklist

## Purpose

Use this checklist before and after any internal filesystem modernization that
touches legacy-facing entry points.

## C ABI: `fs_api_t`

- Do not reorder existing `fs_api_t` fields.
- Do not remove or rename existing function pointers.
- Do not change parameter or return types for existing function pointers.
- Keep `FS_API_VERSION` and `FS_API_CREATEINTERFACE_TAG` changes explicit and
  documented if they ever become unavoidable.
- Keep `GetFSAPI` and `CreateInterface(FS_API_CREATEINTERFACE_TAG, ...)`
  returning a fully populated API table.
- Preserve caller ownership rules for returned buffers and structures,
  including `search_t`, `file_t`, loaded file buffers, and DLL info records.

## C++ ABI: `VFileSystem009`

- Do not rename `FILESYSTEM_INTERFACE_VERSION`.
- Do not reorder virtual methods in `IFileSystem`.
- Do not expose modern C++ implementation types through `VFileSystem009.h`.
- Keep `CreateInterface(FILESYSTEM_INTERFACE_VERSION, ...)` behavior stable,
  including return-value conventions for missing interfaces.

## Modern Internals

- Modern C++ classes must stay behind private adapters.
- Public headers should include legacy types only, not modern helper classes.
- Adapter changes should be covered by wrapper-facing tests when they touch a
  public method.
- Runtime smoke tests should still be run after any change to mounted paths,
  file handles, search results, DLL lookup, or gameinfo hierarchy behavior.

## Required Evidence

For a compatibility-affecting change, record:

- the focused unit test command,
- the full build command,
- any runtime smoke command,
- the files that define the public adapter surface,
- whether `fs_api_t` or `VFileSystem009` signatures changed.
