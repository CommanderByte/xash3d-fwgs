# Windows Filesystem Test Baseline

## Capture Context

Date captured: 2026-05-09.

Branch: `codex-repo-onboarding-modular-rewrite`.

Commit at capture time: `8add44e3`.

SDL2 path:

```text
C:\git\xash3d-fwgs\3rdparty\SDL2_VC
```

## Commands

```powershell
.\waf.bat configure --enable-tests --sdl2=C:\git\xash3d-fwgs\3rdparty\SDL2_VC
.\waf.bat build
```

## Result

Configure completed successfully.

Build completed successfully.

Waf test summary:

```text
tests that pass 14/14
tests that fail 0/14
```

Filesystem tests included in the passing set:

```text
C:\git\xash3d-fwgs\build\filesystem\test_interface.exe
C:\git\xash3d-fwgs\build\filesystem\test_caseinsensitive.exe
C:\git\xash3d-fwgs\build\filesystem\test_no-init.exe
```

Other tests in the passing set:

```text
C:\git\xash3d-fwgs\build\public\test_swapstruct.exe
C:\git\xash3d-fwgs\build\public\test_parsefile.exe
C:\git\xash3d-fwgs\build\public\test_filebase.exe
C:\git\xash3d-fwgs\build\public\test_build.exe
C:\git\xash3d-fwgs\build\public\test_fileext.exe
C:\git\xash3d-fwgs\build\public\test_strings.exe
C:\git\xash3d-fwgs\build\public\test_atlas.exe
C:\git\xash3d-fwgs\build\public\test_atoi.exe
C:\git\xash3d-fwgs\build\public\test_efp.exe
C:\git\xash3d-fwgs\build\3rdparty\MultiEmulator\test_sha256.exe
C:\git\xash3d-fwgs\build\engine\xash_tests.exe
```

## Existing Filesystem Coverage

Current filesystem tests provide:

- `test_interface.exe`: confirms `GetFSAPI`, `CreateInterface("VFileSystem009")`,
  and `CreateInterface("XashFileSystem004")` are available.
- `test_caseinsensitive.exe`: confirms basic case-insensitive directory lookup,
  including files written through the filesystem and files created directly on
  disk.
- `test_no-init.exe`: confirms selected APIs do not fail before `InitStdio`.

## Coverage Gaps Confirmed By Baseline

The current passing tests are useful but narrow. They do not yet cover:

- nested directory case repair
- write path directory creation
- path rejection rules
- direct path behavior
- loose file versus archive precedence
- gamefolder versus basedir precedence
- `gamedironly` filtering
- PAK/ZIP/WAD fixture behavior
- WADs mounted from archives
- `rodir` overlay precedence
- detailed `VFileSystem009` behavior beyond interface lookup
- `FS_FindLibrary` path resolution

These gaps remain the first targets for Phase 2.

## Follow-Up Test Helper Verification

After adding `tests/filesystem/fs_test_common.h` and migrating the existing
filesystem tests to use it, `.\waf.bat build` reran the affected filesystem
tests successfully:

```text
tests that pass 3/3
tests that fail 0/3
```

Passing tests:

```text
C:\git\xash3d-fwgs\build\filesystem\test_interface.exe
C:\git\xash3d-fwgs\build\filesystem\test_caseinsensitive.exe
C:\git\xash3d-fwgs\build\filesystem\test_no-init.exe
```

After moving the test sources to `tests/filesystem` and expanding
`caseinsensitive.c` with nested case repair, cache refresh, write-path creation,
and rejected-path coverage, `.\waf.bat build` again passed the affected
filesystem tests:

```text
tests that pass 3/3
tests that fail 0/3
```

After adding direct-path and generated PAK archive-order coverage,
`.\waf.bat build` passed the expanded filesystem set:

```text
tests that pass 5/5
tests that fail 0/5
```

Passing filesystem tests:

```text
C:\git\xash3d-fwgs\build\filesystem\test_archive-order.exe
C:\git\xash3d-fwgs\build\filesystem\test_interface.exe
C:\git\xash3d-fwgs\build\filesystem\test_caseinsensitive.exe
C:\git\xash3d-fwgs\build\filesystem\test_directpath.exe
C:\git\xash3d-fwgs\build\filesystem\test_no-init.exe
```

After adding hierarchy coverage for gamefolder/basedir ordering and
`gamedironly`, plus generated PK3 coverage for stored and deflated ZIP entries,
`.\waf.bat clean build` passed the full test set:

```text
tests that pass 18/18
tests that fail 0/18
```

Passing filesystem tests:

```text
C:\git\xash3d-fwgs\build\filesystem\test_archive-order.exe
C:\git\xash3d-fwgs\build\filesystem\test_interface.exe
C:\git\xash3d-fwgs\build\filesystem\test_caseinsensitive.exe
C:\git\xash3d-fwgs\build\filesystem\test_directpath.exe
C:\git\xash3d-fwgs\build\filesystem\test_hierarchy.exe
C:\git\xash3d-fwgs\build\filesystem\test_no-init.exe
C:\git\xash3d-fwgs\build\filesystem\test_zip-archive.exe
```

After adding generated WAD3 lump coverage and a generated PAK containing a WAD3,
`.\waf.bat clean build` passed the full test set:

```text
tests that pass 19/19
tests that fail 0/19
```

Passing filesystem tests:

```text
C:\git\xash3d-fwgs\build\filesystem\test_archive-order.exe
C:\git\xash3d-fwgs\build\filesystem\test_interface.exe
C:\git\xash3d-fwgs\build\filesystem\test_caseinsensitive.exe
C:\git\xash3d-fwgs\build\filesystem\test_directpath.exe
C:\git\xash3d-fwgs\build\filesystem\test_hierarchy.exe
C:\git\xash3d-fwgs\build\filesystem\test_no-init.exe
C:\git\xash3d-fwgs\build\filesystem\test_wad-archive.exe
C:\git\xash3d-fwgs\build\filesystem\test_zip-archive.exe
```

After adding `rodir` overlay precedence coverage and expanded
`CreateInterface` behavior checks, `.\waf.bat clean build` passed the full
test set:

```text
tests that pass 20/20
tests that fail 0/20
```

Passing filesystem tests:

```text
C:\git\xash3d-fwgs\build\filesystem\test_archive-order.exe
C:\git\xash3d-fwgs\build\filesystem\test_interface.exe
C:\git\xash3d-fwgs\build\filesystem\test_caseinsensitive.exe
C:\git\xash3d-fwgs\build\filesystem\test_directpath.exe
C:\git\xash3d-fwgs\build\filesystem\test_hierarchy.exe
C:\git\xash3d-fwgs\build\filesystem\test_no-init.exe
C:\git\xash3d-fwgs\build\filesystem\test_rodir.exe
C:\git\xash3d-fwgs\build\filesystem\test_wad-archive.exe
C:\git\xash3d-fwgs\build\filesystem\test_zip-archive.exe
```

## Build Warnings Observed

The build succeeded with existing warnings unrelated to the filesystem tests:

- `engine/client/sound/s_main.c(1120)`: uninitialized local variable
  `ambient_channel` used.
- `engine/common/imagelib/img_png.c(587,590)`: right shift by too large amount.
- `engine/client/cl_sprite.c(80)`: different enum types compared.

Track these separately from filesystem behavior tests unless one blocks a test
run.
