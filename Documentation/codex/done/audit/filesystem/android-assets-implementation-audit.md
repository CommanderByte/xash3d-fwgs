# Android Assets Implementation Migration Audit

This note opens Phase 24 and records the current `filesystem/android.c`
responsibilities before moving target-neutral behavior into
`src/filesystem/android_assets_backend.cpp`.

## Current Shape

`AndroidAssetsBackend` exists as a bridge object. The Android-specific source
file must remain the only place that touches JNI, `AAssetManager`, `AAsset`,
and Android build headers. The modern backend can still own policy and
orchestration by receiving opaque asset handles through runtime callbacks.

```mermaid
flowchart LR
    SearchPath["searchpath_t\nSEARCHPATH_ANDROID_ASSETS"]
    Bridge["Android assets bridge\nfilesystem/android_assets_backend_adapter.cpp"]
    Backend["AndroidAssetsBackend\nsrc/filesystem/android_assets_backend.cpp"]
    Adapter["Android runtime adapter\nfilesystem/android.c"]
    Platform["JNI / AAssetManager / AAsset"]

    SearchPath --> Bridge
    Bridge --> Backend
    Backend --> Adapter
    Adapter --> Platform
```

## Responsibility Map

| Current code | Current role | Target owner |
| --- | --- | --- |
| JNI setup and package lookup | Finds Android context, package names, asset list, and asset manager methods. | Legacy Android adapter only. |
| `Android_ListDirectory` | Calls Java asset listing API and appends strings. | Runtime callback used by modern search helper. |
| `FS_LoadAndroidAssets` | Allocates Android asset searchpath payload and opens root asset directory. | Adapter until searchpath/runtime ownership moves. |
| `FS_FindFile_AndroidAssets_Legacy` | Opens an asset to test existence and copies fixed name. | `AndroidAssetsBackend` helper using opaque open/close callbacks. |
| `FS_Search_AndroidAssets_Legacy` | Splits base path, lists one asset directory, matches pattern, suppresses duplicates. | `AndroidAssetsBackend` search helper. |
| `FS_OpenFile_AndroidAssets_Legacy` | Allocates `file_t`, opens an asset descriptor, and initializes file fields. | `AndroidAssetsBackend` open helper with adapter callbacks for allocation and `file_t` setup. |
| `FS_LoadAndroidAssetsFile_Legacy` | Opens buffered asset, allocates caller-owned buffer, reads bytes, null-terminates result. | `AndroidAssetsBackend` load helper with opaque asset callbacks. |
| `FS_FileTime_AndroidAssets_Legacy` | Synthesizes file time from build commit date. | Adapter for now; depends on build metadata and platform time parsing. |
| `FS_AddAndroidAssets_Fullpath` | Validates flags, chooses engine/game assets, creates searchpath, and registers callbacks. | Adapter entry point until `FilesystemRuntime` owns search paths. |

## Behavioral Quirks To Preserve

- Android assets are mounted only when JNI method lookup succeeds.
- Static and custom path flags reject Android asset mounting.
- Game asset selection depends on `FS_GAMEDIR_PATH` plus basedir/gamefolder
  mismatch.
- Search is one-directory-deep from the pattern base path, like directory
  search.
- Search matching is effectively case-insensitive regardless of the
  `caseinsensitive` argument.
- Loaded asset buffers are null-terminated and report the raw asset length.
- Desktop builds must compile without Android headers or Android runtime
  symbols leaking into `src/filesystem`.

## Phase 24 Progress

- `FS-ANDROID-IMPL-001`: Complete. The Android assets audit is captured in this
  document.
- `FS-ANDROID-IMPL-002`: Complete. Target-neutral find, search, open, and load
  orchestration now lives in `src/filesystem/android_assets_backend.cpp`.
  Android-specific operations remain runtime callbacks in
  `filesystem/android.c`.
- `FS-ANDROID-IMPL-003`: Complete. JNI, `AAssetManager`, and `AAsset` remain
  behind `#if XASH_ANDROID` in `filesystem/android.c`; desktop tests exercise
  the modern helpers through opaque fake asset handles.

## Remaining Blockers

- `android_assets_t` allocation, package-name ownership, root asset directory
  lifetime, and `FS_AddAndroidAssets_Fullpath` remain tied to legacy
  searchpath ownership.
- Synthesized file time still uses legacy build metadata and time parsing.
- A future `FilesystemRuntime` should own mount selection and callback
  registration for Android assets along with the other backend types.
