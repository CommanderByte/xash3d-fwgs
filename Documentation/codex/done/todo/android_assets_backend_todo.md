# Android Assets Backend TODO

## Purpose

This TODO tracks the Android asset backend. It should migrate after desktop
archive backends because it requires Android-specific runtime validation.

## Current Legacy Responsibilities

- Obtain Android asset manager handles from engine native objects.
- Mount APK asset roots as search paths.
- Search, find, open, load, print, and close Android asset entries.

## Migration Order

- [x] Analyze compile and runtime impact before adding bridge code.
  Evidence: `Documentation/codex/modularization-plan/android-assets-backend-plan.md`.
- [x] Keep Android runtime code compiled behind `XASH_ANDROID`.
  Evidence: `filesystem/android.c` remains wrapped by `#if XASH_ANDROID`.
- [x] Add target-neutral `AndroidAssetsBackend` class and hook-forwarding
  tests.
  Evidence: `src/include/filesystem/android_assets_backend.hpp`,
  `src/filesystem/android_assets_backend.cpp`,
  `tests/filesystem/android_assets_backend.cpp`.
- [x] Add C adapter shape without Android headers.
  Evidence: `filesystem/android_assets_backend_adapter.h`,
  `filesystem/android_assets_backend_adapter.cpp`.
- [x] Preserve non-Android builds without requiring Android headers or libs.
  Evidence: the adapter and modern backend include only target-neutral
  filesystem declarations; Android APIs remain in `filesystem/android.c`.
- [x] Route Android asset callbacks through the bridge inside
  `filesystem/android.c`.
  Evidence: `filesystem/android.c`.
- [x] Plan Android runtime validation separately from Windows smoke tests.
  Evidence: `Documentation/codex/modularization-plan/android-assets-backend-plan.md`.

## Boundaries

- Do not require Android runtime dependencies for regular desktop builds.
- Do not change `FS_InitAndroid` or `FS_AddAndroidAssets_Fullpath` public
  internal entry points during early migration.
- Store bridge ownership inside `android_assets_t` rather than changing
  `searchpath_t` layout.
