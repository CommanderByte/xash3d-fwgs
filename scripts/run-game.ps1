param(
    [string]$RuntimeDir = "run-win32",
    [string]$SteamHalfLifePath = "",
    [string]$BaseDir = "",
    [string[]]$GameArgs = @("-dev", "2", "-log"),
    [switch]$SkipRuntimeCopy,
    [switch]$CopyLauncher,
    [switch]$ForceRuntimeCopy,
    [switch]$StopRunningXash,
    [switch]$Wait,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return (Join-Path $RepoRoot $Path)
}

function Format-Argument {
    param([string]$Argument)

    if ($null -eq $Argument) {
        return '""'
    }

    if ($Argument -match '[\s"]') {
        return '"' + ($Argument -replace '"', '\"') + '"'
    }

    return $Argument
}

function Format-CommandLine {
    param(
        [string]$File,
        [string[]]$Arguments
    )

    return (@($File) + ($Arguments | ForEach-Object { Format-Argument $_ })) -join " "
}

function Get-RuntimeXashProcesses {
    param([string]$RuntimePath)

    $runtimeExe = Join-Path $RuntimePath "xash3d.exe"
    return @(Get-Process -Name "xash3d" -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq $runtimeExe })
}

function Ensure-RuntimeNotRunning {
    param([string]$RuntimePath)

    $processes = Get-RuntimeXashProcesses -RuntimePath $RuntimePath
    if ($processes.Count -eq 0) {
        return
    }

    if (-not $StopRunningXash) {
        $ids = ($processes | ForEach-Object { $_.Id }) -join ", "
        throw "Runtime xash3d.exe is already running from $RuntimePath (PID $ids). Close it or rerun with -StopRunningXash."
    }

    foreach ($process in $processes) {
        Write-Host ("Stopping runtime xash3d.exe PID {0}" -f $process.Id)
        if (-not $DryRun) {
            Stop-Process -Id $process.Id -Force
        }
    }

    if (-not $DryRun) {
        Start-Sleep -Milliseconds 500
    }
}

$RuntimePath = Resolve-RepoPath $RuntimeDir
if (-not (Test-Path -LiteralPath $RuntimePath)) {
    throw "Runtime directory does not exist: $RuntimePath"
}

if ([string]::IsNullOrWhiteSpace($SteamHalfLifePath)) {
    if (-not [string]::IsNullOrWhiteSpace($env:XASH3D_RODIR)) {
        $SteamHalfLifePath = $env:XASH3D_RODIR
    } else {
        $SteamHalfLifePath = "C:\Program Files (x86)\Steam\steamapps\common\Half-Life"
    }
}

if ([string]::IsNullOrWhiteSpace($BaseDir)) {
    $BaseDir = $RuntimePath
} else {
    $BaseDir = Resolve-RepoPath $BaseDir
}

if (-not (Test-Path -LiteralPath $SteamHalfLifePath)) {
    throw "Steam Half-Life path does not exist: $SteamHalfLifePath"
}

$gfxWad = Join-Path $SteamHalfLifePath "valve\gfx.wad"
if (-not (Test-Path -LiteralPath $gfxWad)) {
    Write-Warning "Could not find $gfxWad. The game may fail with 'couldn't load gfx.wad'."
}

$exe = Join-Path $RuntimePath "xash3d.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Runtime executable does not exist: $exe"
}

Ensure-RuntimeNotRunning -RuntimePath $RuntimePath

if (-not $SkipRuntimeCopy) {
    $refreshArgs = @{
        RuntimeDir = $RuntimePath
    }

    if ($CopyLauncher) {
        $refreshArgs.CopyLauncher = $true
    }

    if ($ForceRuntimeCopy) {
        $refreshArgs.Force = $true
    }

    if ($DryRun) {
        $refreshArgs.DryRun = $true
    }

    Write-Host "== Refresh runtime binaries =="
    Push-Location $RepoRoot
    try {
        & (Join-Path $PSScriptRoot "refresh-runtime-binaries.ps1") @refreshArgs
    } finally {
        Pop-Location
    }
    Write-Host ""
}

$previousBaseDir = $env:XASH3D_BASEDIR
$previousRodir = $env:XASH3D_RODIR
$logPath = Join-Path $RuntimePath "engine.log"

try {
    $env:XASH3D_BASEDIR = $BaseDir
    $env:XASH3D_RODIR = $SteamHalfLifePath

    Write-Host "== Launch game =="
    Write-Host ("XASH3D_BASEDIR={0}" -f $env:XASH3D_BASEDIR)
    Write-Host ("XASH3D_RODIR={0}" -f $env:XASH3D_RODIR)
    Write-Host (">> {0}" -f (Format-CommandLine $exe $GameArgs))
    Write-Host ("Log: {0}" -f $logPath)

    if ($DryRun) {
        Write-Host "Dry run only; not starting the game."
        exit 0
    }

    $argumentLine = ($GameArgs | ForEach-Object { Format-Argument $_ }) -join " "
    $process = Start-Process -FilePath $exe `
        -ArgumentList $argumentLine `
        -WorkingDirectory $RuntimePath `
        -PassThru

    Write-Host ("Started xash3d.exe PID {0}" -f $process.Id)

    if ($Wait) {
        $process.WaitForExit()
        Write-Host ("xash3d.exe exited with code {0}" -f $process.ExitCode)
        exit $process.ExitCode
    }
} finally {
    $env:XASH3D_BASEDIR = $previousBaseDir
    $env:XASH3D_RODIR = $previousRodir
}
