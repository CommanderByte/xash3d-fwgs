# Android Assets Backend Plan

## Purpose

This plan covers the final search-path backend bridge after the desktop
Directory, PAK, WAD, and ZIP/PK3 pilots. Android assets are different because
they depend on JNI and Android `AAssetManager` runtime handles, so the migration
must preserve desktop builds while preparing a clean Android-only adapter.

## Current Shape

The legacy implementation lives in `filesystem/android.c` and is guarded by
`#if XASH_ANDROID`. It owns three responsibilities at once:

- JNI discovery through `FS_InitAndroid`.
- Android asset manager lifetime through `android_assets_t`.
- Search-path callbacks for print, close, open, file time, find, search, and
  direct load.

The search path type is already modeled as `SEARCHPATH_ANDROID_ASSETS`, and the
modern backend enum already has `SearchPathBackendType::AndroidAssets`.

## Impact Analysis

### Desktop Builds

Desktop builds should not include Android headers, link Android libraries, or
call Android APIs. The current source already protects runtime Android code
behind `#if XASH_ANDROID`, and that boundary should remain intact.

The new pure C++ backend class can compile on all targets if its public header
only depends on `search_path_backend.hpp` and legacy opaque types such as
`file_t` and `stringlist_t`. It must not include `<jni.h>`,
`<android/asset_manager.h>`, or `filesystem_internal.h` for Android-only fields.

### Android Builds

Android builds currently compile `filesystem/android.c`, link `libandroid`, and
call `FS_InitAndroid` during filesystem startup. The adapter should preserve
that call flow and keep `FS_AddAndroidAssets_Fullpath` as the legacy C entry
point used by `filesystem.c`.

The bridge should be behavior-neutral: it wraps existing callbacks first, then
future work can move logic into the modern backend once Android runtime testing
is available.

### Build System

`filesystem/wscript` glob-builds `filesystem/*.c` and `filesystem/*.cpp`.
`src/wscript` glob-builds `src/filesystem/*.cpp` into
`modern_filesystem_debug`.

This means:

- `src/filesystem/android_assets_backend.cpp` should compile everywhere.
- `filesystem/android_assets_backend_adapter.cpp` may also compile everywhere,
  but any use of `search->assets`, JNI, or Android headers must be guarded or
  avoided outside `XASH_ANDROID`.
- `filesystem/android.c` should remain the only file that talks directly to JNI
  and Android asset APIs during the first bridge pass.

## Proposed Backend Shape

Add a modern backend class shaped like the existing archive bridges:

```cpp
namespace xash::filesystem {

struct AndroidAssetsBackendOps {
	void *context;
	void (*close)(void *context);
	void (*printInfo)(void *context, char *dst, size_t size);
	file_t *(*openFile)(void *context, const char *path, const char *mode, int index);
	int (*fileTime)(void *context, const char *path);
	int (*findFile)(void *context, const char *path, char *fixedName, size_t len);
	void (*search)(void *context, stringlist_t *list, const char *pattern,
		bool caseInsensitive);
	byte *(*loadFile)(void *context, const char *path, int index,
		fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *));
};

class AndroidAssetsBackend final : public ISearchPathBackend {
	// Same behavior-neutral forwarding model as WAD and ZIP.
};

}
```

The class should be target-neutral. Android-specific behavior stays in the hook
provider until there is an Android validation loop.

## Adapter Plan

The adapter should mirror the WAD/ZIP bridge because Android assets also have a
custom `pfnLoadFile` path:

- `filesystem/android_assets_backend_adapter.h`
- `filesystem/android_assets_backend_adapter.cpp`
- `src/include/filesystem/android_assets_backend.hpp`
- `src/filesystem/android_assets_backend.cpp`
- `tests/filesystem/android_assets_backend.cpp`

The adapter API can compile everywhere because it only needs opaque legacy
types. The call site in `filesystem/android.c` should remain behind
`#if XASH_ANDROID`.

## Migration Steps

1. Add the target-neutral backend class and focused unit tests for default
   inert behavior and hook forwarding.
2. Add the C bridge files with no Android headers.
3. In `filesystem/android.c`, rename legacy callbacks to `*_Legacy`, create a
   bridge handle when `FS_AddAndroidAssets_Fullpath` succeeds, and forward
   callbacks through the bridge.
4. Store the bridge handle inside `android_assets_t` rather than changing
   `searchpath_t` layout.
5. Destroy the bridge before freeing `android_assets_t`.
6. Run desktop build/tests to prove non-Android builds are unaffected.
7. Defer runtime validation to an Android build/device or Android CI job.

## Validation

Desktop validation:

- `.\waf.bat build`
- Focused backend unit test target for `AndroidAssetsBackend`.
- Windows smoke test to prove the extra cross-platform source files do not
  change desktop filesystem behavior.

Android validation, when available:

- Configure and build an Android target.
- Launch with APK assets mounted.
- Confirm `fs_path` includes Android asset roots.
- Confirm `FS_LoadFile`, `FS_Search`, and direct file open work for bundled APK
  assets.
- Confirm missing JNI methods fail softly and do not crash startup.

## Non-Goals

- Do not move JNI discovery out of `filesystem/android.c` in the first pass.
- Do not require Android SDK/NDK headers for desktop builds.
- Do not change `FS_InitAndroid` or `FS_AddAndroidAssets_Fullpath` signatures.
- Do not add Android runtime tests to the Windows smoke loop.
