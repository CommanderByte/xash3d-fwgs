#requires -Version 5
<#
    regen.ps1 — rebuild tests/goldens/studio_math_goldens.inc from the verbatim
    legacy studio-math kernels. Dev-only (Q-18 cross-check harness). See README.md.

    Resolves the MSVC toolchain via XASH_VSDEVCMD (override) else vswhere, then
    compiles gen.c + public/xash3d_mathlib.c + public/matrixlib.c as C and runs
    the result to (re)emit the committed .inc. The build/ subdir is gitignored.
#>
$ErrorActionPreference = 'Stop'

$toolDir  = $PSScriptRoot
$root     = (Resolve-Path (Join-Path $toolDir '..\..\..')).Path
$outDir   = Join-Path $root 'xash3dpp\tests\goldens'
$out      = Join-Path $outDir 'studio_math_goldens.inc'
$buildDir = Join-Path $toolDir 'build'

New-Item -ItemType Directory -Force -Path $outDir, $buildDir | Out-Null

# --- resolve VsDevCmd.bat -------------------------------------------------
$vsdevcmd = $env:XASH_VSDEVCMD
if ( -not $vsdevcmd ) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if ( -not (Test-Path $vswhere) ) { throw 'vswhere not found; set XASH_VSDEVCMD to your VsDevCmd.bat' }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ( -not $vsPath ) { throw 'no VS install with C++ tools found; set XASH_VSDEVCMD' }
    $vsdevcmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
}
if ( -not (Test-Path $vsdevcmd) ) { throw "VsDevCmd.bat not found at $vsdevcmd" }

# --- compose a build+run batch (avoids PS/cmd nested-quote hazards) -------
$gen  = Join-Path $toolDir 'gen.c'
$ml   = Join-Path $root 'public\xash3d_mathlib.c'
$mtx  = Join-Path $root 'public\matrixlib.c'

$bat = @"
@echo off
call "$vsdevcmd" -arch=amd64 -no_logo || exit /b 1
cl /nologo /O2 /Gy /TC /I"$root\public" /I"$root\common" /I"$root\engine" /I"$root\3rdparty\library_suffix\include" "$gen" "$ml" "$mtx" /Fo:"$buildDir\\" /Fe:"$buildDir\gen.exe" /link /OPT:REF || exit /b 1
"$buildDir\gen.exe" "$out" || exit /b 1
"@
$batPath = Join-Path $buildDir 'build_and_run.bat'
Set-Content -Path $batPath -Value $bat -Encoding ASCII

cmd /c "`"$batPath`""
if ( $LASTEXITCODE -ne 0 ) { throw "regen failed (exit $LASTEXITCODE)" }

Write-Host "Regenerated $out"
