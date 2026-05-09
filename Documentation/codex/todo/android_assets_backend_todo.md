# Android Assets Backend TODO

## Purpose

This TODO tracks the Android asset backend. It should migrate after desktop
archive backends because it requires Android-specific runtime validation.

## Current Legacy Responsibilities

- Obtain Android asset manager handles from engine native objects.
- Mount APK asset roots as search paths.
- Search, find, open, load, print, and close Android asset entries.

## Migration Order

- [ ] Keep Android code compiled behind `XASH_ANDROID`.
- [ ] Add adapter shape only after desktop backend bridge pattern is stable.
- [ ] Preserve non-Android builds without requiring Android headers or libs.
- [ ] Plan Android runtime validation separately from Windows smoke tests.

## Boundaries

- Do not require Android runtime dependencies for regular desktop builds.
- Do not change `FS_InitAndroid` or `FS_AddAndroidAssets_Fullpath` public
  internal entry points during early migration.
