param(
    [string]$SteamHalfLifePath = "C:\Program Files (x86)\Steam\steamapps\common\Half-Life",
    [string]$SdlVersion = "2.32.10",
    [string]$RuntimeDir = "run-win32",
    [switch]$SkipEngineBuild,
    [switch]$SkipHlsdkBuild
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$CacheDir = Join-Path $RepoRoot ".codex-cache"
$SdlDir = Join-Path $RepoRoot "3rdparty\SDL2_VC"
$HlsdkDir = Join-Path $RepoRoot "3rdparty\hlsdk-portable"
$RuntimePath = Join-Path $RepoRoot $RuntimeDir

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $vsCmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path -LiteralPath $vsCmake) {
        return $vsCmake
    }

    throw "CMake was not found on PATH or at the Visual Studio 2022 bundled CMake path."
}

function Ensure-Sdl2 {
    if (Test-Path -LiteralPath (Join-Path $SdlDir "include\SDL.h")) {
        Write-Host "SDL2 already present at $SdlDir"
        return
    }

    New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null

    $zipPath = Join-Path $CacheDir "SDL2-devel-$SdlVersion-VC.zip"
    $url = "https://github.com/libsdl-org/SDL/releases/download/release-$SdlVersion/SDL2-devel-$SdlVersion-VC.zip"
    Write-Host "Downloading SDL2 $SdlVersion..."
    Invoke-WebRequest -Uri $url -OutFile $zipPath

    $extractDir = Join-Path $CacheDir "SDL2-$SdlVersion"
    if (Test-Path -LiteralPath $extractDir) {
        Remove-Item -LiteralPath $extractDir -Recurse -Force
    }

    Expand-Archive -LiteralPath $zipPath -DestinationPath $extractDir
    $unpacked = Join-Path $extractDir "SDL2-$SdlVersion"
    if (-not (Test-Path -LiteralPath $unpacked)) {
        throw "Unexpected SDL2 archive layout: $unpacked not found."
    }

    if (Test-Path -LiteralPath $SdlDir) {
        Remove-Item -LiteralPath $SdlDir -Recurse -Force
    }

    Move-Item -LiteralPath $unpacked -Destination $SdlDir
}

function Ensure-Hlsdk {
    if (Test-Path -LiteralPath (Join-Path $HlsdkDir ".git")) {
        Write-Host "hlsdk-portable already present at $HlsdkDir"
        return
    }

    git clone --depth 1 https://github.com/FWGS/hlsdk-portable.git $HlsdkDir
}

Push-Location $RepoRoot
try {
    if (-not (Test-Path -LiteralPath $SteamHalfLifePath)) {
        throw "Steam Half-Life path does not exist: $SteamHalfLifePath"
    }

    Ensure-Sdl2
    Ensure-Hlsdk

    if (-not $SkipEngineBuild) {
        & .\waf.bat configure "--sdl2=$SdlDir"
        & .\waf.bat build
        & .\waf.bat install "--destdir=$RuntimePath"
    }

    New-Item -ItemType Directory -Force -Path $RuntimePath | Out-Null
    Copy-Item -LiteralPath (Join-Path $SdlDir "lib\x86\SDL2.dll") -Destination (Join-Path $RuntimePath "SDL2.dll") -Force

    if (-not $SkipHlsdkBuild) {
        $cmake = Find-CMake
        Push-Location $HlsdkDir
        try {
            & $cmake -G "Visual Studio 17 2022" -A Win32 -B build -S .
            & $cmake --build build --config Release
        }
        finally {
            Pop-Location
        }
    }

    $valveDir = Join-Path $RuntimePath "valve"
    New-Item -ItemType Directory -Force -Path (Join-Path $valveDir "dlls") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $valveDir "cl_dlls") | Out-Null

    Copy-Item -LiteralPath (Join-Path $HlsdkDir "build\dlls\Release\hl.dll") -Destination (Join-Path $valveDir "dlls\hl.dll") -Force
    Copy-Item -LiteralPath (Join-Path $HlsdkDir "build\cl_dll\Release\client.dll") -Destination (Join-Path $valveDir "cl_dlls\client.dll") -Force

    @"
title "Half-Life"
basedir "valve"
startmap "c0a0"
trainmap "t0a0"
mp_entity "info_player_deathmatch"
gamedll "dlls/hl.dll"
gamedll_linux "dlls/hl.so"
gamedll_osx "dlls/hl.dylib"
secure 1
gamemode "singleplayer_only"
type "Single"
animated_title 1
hd_background 1
internal_vgui_support 1
"@ | Set-Content -LiteralPath (Join-Path $valveDir "gameinfo.txt") -Encoding ASCII

    Write-Host ""
    Write-Host "Runtime ready at $RuntimePath"
    Write-Host "Launch with:"
    Write-Host "  `$env:XASH3D_BASEDIR = '$RuntimePath'"
    Write-Host "  `$env:XASH3D_RODIR = '$SteamHalfLifePath'"
    Write-Host "  Push-Location '$RuntimePath'; .\xash3d.exe -dev 2 -log; Pop-Location"
}
finally {
    Pop-Location
}
