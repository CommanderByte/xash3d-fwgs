param(
    [string]$RuntimeDir = "run-win32",
    [string]$SteamHalfLifePath = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$RuntimePath = if ([System.IO.Path]::IsPathRooted($RuntimeDir)) {
    $RuntimeDir
} else {
    Join-Path $RepoRoot $RuntimeDir
}

if ([string]::IsNullOrWhiteSpace($SteamHalfLifePath)) {
    if (-not [string]::IsNullOrWhiteSpace($env:XASH3D_RODIR)) {
        $SteamHalfLifePath = $env:XASH3D_RODIR
    } else {
        $SteamHalfLifePath = "C:\Program Files (x86)\Steam\steamapps\common\Half-Life"
    }
}

function Show-FilePair {
    param(
        [string]$Name,
        [string]$BuildPath,
        [string]$RuntimePathValue
    )

    $buildExists = Test-Path -LiteralPath $BuildPath
    $runtimeExists = Test-Path -LiteralPath $RuntimePathValue
    Write-Host "${Name}:"
    Write-Host ("  build:   {0}" -f ($(if ($buildExists) { (Get-Item -LiteralPath $BuildPath).LastWriteTime } else { "missing" })))
    Write-Host ("  runtime: {0}" -f ($(if ($runtimeExists) { (Get-Item -LiteralPath $RuntimePathValue).LastWriteTime } else { "missing" })))
}

Write-Host "== Runtime Paths =="
Write-Host "repo:    $RepoRoot"
Write-Host "runtime: $RuntimePath"
Write-Host "rodir:   $SteamHalfLifePath"
Write-Host ("runtime exists: {0}" -f (Test-Path -LiteralPath $RuntimePath))
Write-Host ("rodir exists:   {0}" -f (Test-Path -LiteralPath $SteamHalfLifePath))

Write-Host ""
Write-Host "== Running Processes =="
$runtimeExe = Join-Path $RuntimePath "xash3d.exe"
$processes = @(Get-Process -Name "xash3d" -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $runtimeExe })
if ($processes.Count -eq 0) {
    Write-Host "No xash3d.exe process is using this runtime."
} else {
    $processes | Select-Object Id, ProcessName, Path, StartTime | Format-Table -AutoSize
}

Write-Host ""
Write-Host "== Build vs Runtime Binaries =="
Show-FilePair "xash.dll" (Join-Path $RepoRoot "build/engine/xash.dll") (Join-Path $RuntimePath "xash.dll")
Show-FilePair "filesystem_stdio.dll" (Join-Path $RepoRoot "build/filesystem/filesystem_stdio.dll") (Join-Path $RuntimePath "filesystem_stdio.dll")
Show-FilePair "ref_gl.dll" (Join-Path $RepoRoot "build/ref/gl/ref_gl.dll") (Join-Path $RuntimePath "ref_gl.dll")

Write-Host ""
Write-Host "== Asset Checks =="
$gfxWad = Join-Path $SteamHalfLifePath "valve/gfx.wad"
Write-Host ("Steam gfx.wad: {0}" -f ($(if (Test-Path -LiteralPath $gfxWad) { $gfxWad } else { "missing" })))
$gameinfo = Join-Path $RuntimePath "valve/gameinfo.txt"
Write-Host ("Runtime gameinfo: {0}" -f ($(if (Test-Path -LiteralPath $gameinfo) { $gameinfo } else { "missing" })))

Write-Host ""
Write-Host "== Last Engine Log =="
$engineLog = Join-Path $RuntimePath "engine.log"
if (-not (Test-Path -LiteralPath $engineLog)) {
    Write-Host "No engine.log found."
    exit 0
}

$log = Get-Content -LiteralPath $engineLog -Raw
if ($log -match "Time to first frame:\s*([0-9.]+)\s*seconds") {
    Write-Host ("first frame: {0} seconds" -f $Matches[1])
} else {
    Write-Host "first frame: not found"
}

if ($log -match "Stopped with reason `"([^`"]+)`"") {
    Write-Host ("stop reason: {0}" -f $Matches[1])
} else {
    Write-Host "stop reason: not found"
}

if ($log -match "couldn't load gfx\.wad") {
    Write-Host "gfx.wad issue: present in log"
} else {
    Write-Host "gfx.wad issue: not present"
}

if ($log -match "Crash:\s*address\s+(.+)") {
    Write-Host ("crash: {0}" -f $Matches[1])
    $tail = Get-Content -LiteralPath $engineLog | Select-Object -Last 25
    Write-Host "last crash/log lines:"
    $tail | ForEach-Object { Write-Host "  $_" }
} else {
    Write-Host "crash: not present"
}
