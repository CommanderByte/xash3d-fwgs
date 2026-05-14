# Compatibility Facades TODO

## Purpose

Track legacy-facing wrappers that should remain stable while internals move to
modern helpers.

## Migration Order

- [x] Keep `fs_api_t` as the C ABI facade.
- [x] Keep `VFileSystem009` as the C++ ABI facade.
- [x] Add ABI drift checklist before larger rewires.
- [x] Add wrapper-only tests when internal helper changes touch public methods.

## Boundaries

- Do not rename `FILESYSTEM_INTERFACE_VERSION`.
- Do not expose modern C++ classes through public headers.
- Treat wrapper changes as compatibility work, not modernization playgrounds.
