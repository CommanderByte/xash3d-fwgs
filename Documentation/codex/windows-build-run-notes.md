# Windows Build And Run Notes

These notes capture the first Windows smoke test for this fork on branch
`codex-repo-onboarding-modular-rewrite`.

## Build Baseline

The dedicated-server build works with the repo defaults:

```powershell
.\waf.bat configure --dedicated
.\waf.bat build
```

The regular Windows client build works with SDL2 from the official Visual C++
development package placed under `3rdparty/SDL2_VC`:

```powershell
.\waf.bat configure --sdl2=C:\git\xash3d-fwgs\3rdparty\SDL2_VC
.\waf.bat build
.\waf.bat install --destdir=C:\git\xash3d-fwgs\run-win32
Copy-Item .\3rdparty\SDL2_VC\lib\x86\SDL2.dll .\run-win32\SDL2.dll
```

Run from the installed layout, not directly from `build/src`, because
`xash3d.exe` expects `xash.dll` and related runtime DLLs beside it.

The repeatable setup script is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup-windows-runtime.ps1
```

It downloads SDL2 into `3rdparty/SDL2_VC`, clones `hlsdk-portable` into
`3rdparty/hlsdk-portable`, builds the engine, installs `run-win32`, builds the
HLSDK DLLs, and writes the local `gameinfo.txt`.

## Steam Asset Layout

The local Half-Life Steam install was used only as a read-only asset source:

```powershell
$env:XASH3D_BASEDIR = 'C:\git\xash3d-fwgs\run-win32'
$env:XASH3D_RODIR = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life'
```

`run-win32/valve/gameinfo.txt` points the engine at local game DLL names:

```text
gamedll "dlls/hl.dll"
gamedll_linux "dlls/hl.so"
gamedll_osx "dlls/hl.dylib"
internal_vgui_support 1
```

## Stock Steam DLL Crash

Using the stock Steam `valve/cl_dlls/client.dll` and `valve/dlls/hl.dll`
caused a startup access violation after renderer, menu, audio, and game assets
had already begun loading.

The useful crash signature was:

```text
vgui::TextImage::getFont (vgui.dll)
HUD_GetStudioModelInterface (client.dll)
HUD_Init (client.dll)
CL_LoadProgs
```

Swapping `vgui.dll` alone did not fix this. Treat this as a client DLL/VGUI
compatibility issue rather than an engine build failure.

## Working Runtime

The successful runtime used Steam assets plus FWGS-compatible game and client
DLLs built from `hlsdk-portable`.

Configure and build `hlsdk-portable` with the Visual Studio CMake bundled with
VS 2022:

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
Push-Location .\3rdparty\hlsdk-portable
& $cmake -G 'Visual Studio 17 2022' -A Win32 -B build -S .
& $cmake --build build --config Release
Pop-Location
```

Copy the built DLLs into the local runtime:

```powershell
Copy-Item .\3rdparty\hlsdk-portable\build\dlls\Release\hl.dll .\run-win32\valve\dlls\hl.dll -Force
Copy-Item .\3rdparty\hlsdk-portable\build\cl_dll\Release\client.dll .\run-win32\valve\cl_dlls\client.dll -Force
```

Launch recipe:

```powershell
Push-Location .\run-win32
$env:XASH3D_BASEDIR = 'C:\git\xash3d-fwgs\run-win32'
$env:XASH3D_RODIR = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life'
.\xash3d.exe -dev 2 -log
Pop-Location
```

This reached the menu visually and the log showed first-frame success before a
clean shutdown:

```text
Time to first frame: 0.489 seconds
LoadBackground: found steam background in game directory
Stopped with reason "command"
```

## Directory Backend Bridge Smoke

After adding the first live directory backend bridge, refresh the local runtime
filesystem DLL from the current build:

```powershell
Copy-Item .\build\filesystem\filesystem_stdio.dll .\run-win32\filesystem_stdio.dll -Force
```

Then run a noninteractive filesystem-path smoke:

```powershell
Push-Location .\run-win32
$env:XASH3D_BASEDIR = 'C:\git\xash3d-fwgs\run-win32'
$env:XASH3D_RODIR = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life'
.\xash3d.exe -dev 2 -log +fs_path +quit
Pop-Location
```

On 2026-05-09 this exited with code `0`, printed the expected Steam `valve`
directory and WAD search paths, and stopped with reason `"command"`.

## Notes For Later

- CMake was available through Visual Studio, but not on `PATH`.
- The build produced a few warnings worth revisiting separately, including
  `ambient_channel` in `engine/client/sound/s_main.c` and several enum or macro
  warnings.
- `run-win32` is a generated smoke-test runtime. Decide later whether the fork
  should keep a scripted runtime setup or keep this folder untracked.
