param(
    [string]$FocusedTarget = "",
    [string]$RuntimeDir = "run-win32",
    [string]$SteamHalfLifePath = "",
    [string]$BaseDir = "",
    [string[]]$SmokeArgs = @("-dev", "2", "-log", "+fs_path", "+wait", "+wait", "+quit"),
    [string]$ReportPath = "",
    [switch]$SkipFocused,
    [switch]$SkipXashBuild,
    [switch]$SkipFullTests,
    [switch]$SkipRuntimeCopy,
    [switch]$SkipSmoke,
    [switch]$CopyLauncher,
    [switch]$AllowSmokeNonZeroExit,
    [switch]$StopRunningXash,
    [switch]$NoReportFile
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

if ([string]::IsNullOrWhiteSpace($BaseDir)) {
    $BaseDir = $RuntimePath
}

if ([string]::IsNullOrWhiteSpace($ReportPath) -and -not $NoReportFile) {
    $reportDir = Join-Path $RepoRoot ".codex-cache\validation-reports"
    $ReportPath = Join-Path $reportDir ("phase-validation-{0}.md" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
}

$Results = @()
$FocusedTestSummary = ""
$FullTestSummary = ""
$SmokeFirstFrame = ""
$SmokeStopReason = ""
$SmokeLogPath = ""
$ExitCode = 0

function Format-CommandLine {
    param(
        [string]$File,
        [string[]]$Arguments
    )

    $parts = @($File) + $Arguments
    return ($parts -join " ")
}

function Invoke-NativeCommand {
    param(
        [string]$File,
        [string[]]$Arguments
    )

    Write-Host (">> {0}" -f (Format-CommandLine $File $Arguments))
    & $File @Arguments 2>&1 | Tee-Object -Variable commandOutput
    $nativeExitCode = $LASTEXITCODE
    $lines = @($commandOutput | Where-Object { $null -ne $_ } | ForEach-Object { $_.ToString() })

    if ($nativeExitCode -ne 0) {
        throw ("Command failed with exit code {0}: {1}" -f $nativeExitCode, (Format-CommandLine $File $Arguments))
    }

    return $lines
}

function Get-TestPassSummary {
    param([string[]]$Lines)

    foreach ($line in $Lines) {
        if ($line -match "tests that pass\s+([0-9]+/[0-9]+)") {
            return ("tests passed {0}" -f $Matches[1])
        }
    }

    return "completed"
}

function Invoke-ValidationStep {
    param(
        [string]$Name,
        [scriptblock]$Body
    )

    Write-Host ""
    Write-Host ("== {0} ==" -f $Name)
    $started = Get-Date
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $status = "PASS"
    $details = ""
    $message = ""

    try {
        $detailOutput = & $Body
        if ($detailOutput) {
            $details = ($detailOutput -join "`n")
        }
    } catch {
        $status = "FAIL"
        $message = $_.Exception.Message
    } finally {
        $timer.Stop()
        $script:Results += [pscustomobject]@{
            Name = $Name
            Status = $status
            DurationSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
            Started = $started
            Details = $details
            Message = $message
        }
    }

    if ($status -ne "PASS") {
        throw $message
    }

    if (-not [string]::IsNullOrWhiteSpace($details)) {
        Write-Host $details
    }
}

function Get-RuntimeXashProcesses {
    if (-not (Test-Path -LiteralPath $RuntimePath)) {
        return @()
    }

    $runtimeExe = Join-Path $RuntimePath "xash3d.exe"
    return @(Get-Process -Name "xash3d" -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq $runtimeExe })
}

function Ensure-RuntimeNotRunning {
    $processes = Get-RuntimeXashProcesses
    if ($processes.Count -eq 0) {
        return
    }

    if (-not $StopRunningXash) {
        $ids = ($processes | ForEach-Object { $_.Id }) -join ", "
        throw "Runtime xash3d.exe is still running from $RuntimePath (PID $ids). Close it or rerun with -StopRunningXash."
    }

    foreach ($process in $processes) {
        Write-Host ("Stopping runtime xash3d.exe PID {0}" -f $process.Id)
        Stop-Process -Id $process.Id -Force
    }

    Start-Sleep -Milliseconds 500
}

function Copy-FirstExisting {
    param(
        [string[]]$Sources,
        [string]$Destination
    )

    foreach ($source in $Sources) {
        $sourcePath = if ([System.IO.Path]::IsPathRooted($source)) {
            $source
        } else {
            Join-Path $RepoRoot $source
        }

        if (Test-Path -LiteralPath $sourcePath) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
            Copy-Item -LiteralPath $sourcePath -Destination $Destination -Force
            return $sourcePath
        }
    }

    return ""
}

function Refresh-RuntimeBinaries {
    Ensure-RuntimeNotRunning

    if (-not (Test-Path -LiteralPath $RuntimePath)) {
        throw "Runtime directory does not exist: $RuntimePath"
    }

    $copied = @()

    $pairs = @(
        @{ Destination = "xash.dll"; Sources = @("build\engine\xash.dll") },
        @{ Destination = "xash.pdb"; Sources = @("build\engine\xash.pdb") },
        @{ Destination = "filesystem_stdio.dll"; Sources = @("build\filesystem\filesystem_stdio.dll") },
        @{ Destination = "filesystem_stdio.pdb"; Sources = @("build\filesystem\filesystem_stdio.pdb") },
        @{ Destination = "ref_gl.dll"; Sources = @("build\ref\gl\ref_gl.dll") },
        @{ Destination = "ref_gl.pdb"; Sources = @("build\ref\gl\ref_gl.pdb") }
    )

    if ($CopyLauncher) {
        $pairs = @(
            @{ Destination = "xash3d.exe"; Sources = @("build\src\xash3d.exe", "build\game_launch\xash3d.exe") },
            @{ Destination = "xash3d.pdb"; Sources = @("build\src\xash3d.pdb", "build\game_launch\xash3d.pdb") }
        ) + $pairs
    }

    foreach ($pair in $pairs) {
        $destination = Join-Path $RuntimePath $pair.Destination
        $source = Copy-FirstExisting -Sources $pair.Sources -Destination $destination
        if (-not [string]::IsNullOrWhiteSpace($source)) {
            $copied += ("{0} -> {1}" -f $source, $destination)
        }
    }

    if ($copied.Count -eq 0) {
        throw "No build outputs were copied into $RuntimePath."
    }

    return $copied
}

function Invoke-SmokeTest {
    if (-not (Test-Path -LiteralPath $SteamHalfLifePath)) {
        throw "Steam Half-Life path does not exist: $SteamHalfLifePath"
    }

    $exe = Join-Path $RuntimePath "xash3d.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Runtime executable does not exist: $exe"
    }

    $previousBaseDir = $env:XASH3D_BASEDIR
    $previousRodir = $env:XASH3D_RODIR
    $script:SmokeLogPath = Join-Path $RuntimePath "engine.log"

    Push-Location $RuntimePath
    $smokeExitError = ""
    try {
        $env:XASH3D_BASEDIR = $BaseDir
        $env:XASH3D_RODIR = $SteamHalfLifePath
        try {
            Invoke-NativeCommand ".\xash3d.exe" $SmokeArgs | Out-Null
        } catch {
            $smokeExitError = $_.Exception.Message
        }
    } finally {
        Pop-Location
        $env:XASH3D_BASEDIR = $previousBaseDir
        $env:XASH3D_RODIR = $previousRodir
    }

    if (-not (Test-Path -LiteralPath $script:SmokeLogPath)) {
        throw "Smoke log was not created: $script:SmokeLogPath"
    }

    $log = Get-Content -LiteralPath $script:SmokeLogPath -Raw
    if ($log -match "couldn't load gfx\.wad") {
        throw "Smoke failed before first frame: gfx.wad was not loaded. Check XASH3D_RODIR and runtime assets."
    }

    if ($log -match "Time to first frame:\s*([0-9.]+)\s*seconds") {
        $script:SmokeFirstFrame = $Matches[1]
    } else {
        throw "Smoke log did not contain a first-frame timing."
    }

    if ($log -match "Stopped with reason `"([^`"]+)`"") {
        $script:SmokeStopReason = $Matches[1]
    } else {
        $script:SmokeStopReason = "unknown"
    }

    $gfxWadPattern = [regex]::Escape("gfx.wad")
    if ($log -notmatch $gfxWadPattern) {
        throw "Smoke reached first frame, but fs_path output did not mention gfx.wad."
    }

    $detail = "first frame {0} seconds; stopped with reason {1}" -f $script:SmokeFirstFrame, $script:SmokeStopReason

    if (-not [string]::IsNullOrWhiteSpace($smokeExitError)) {
        $detail = "$detail; process reported: $smokeExitError"
        if (-not $AllowSmokeNonZeroExit) {
            throw $detail
        }
    }

    return $detail
}

function Write-ValidationReport {
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $status = if ($script:ExitCode -eq 0) { "PASS" } else { "FAIL" }
    $lines = @()

    $lines += "# Phase Validation Report"
    $lines += ""
    $lines += "- Timestamp: $timestamp"
    $lines += "- Status: $status"
    $lines += "- Repo: $RepoRoot"
    $lines += "- Runtime: $RuntimePath"
    $lines += "- XASH3D_BASEDIR: $BaseDir"
    $lines += "- XASH3D_RODIR: $SteamHalfLifePath"
    $lines += ""
    $lines += "## Steps"
    $lines += ""

    foreach ($result in $script:Results) {
        $lines += "- $($result.Status) $($result.Name) ($($result.DurationSeconds)s)"
        if (-not [string]::IsNullOrWhiteSpace($result.Details)) {
            $lines += "  - $($result.Details)"
        }
        if (-not [string]::IsNullOrWhiteSpace($result.Message)) {
            $lines += "  - $($result.Message)"
        }
    }

    $evidence = @()
    if (-not [string]::IsNullOrWhiteSpace($FocusedTarget) -and -not $SkipFocused) {
        $evidence += "``.\waf.bat build --targets=$FocusedTarget`` passed"
    }
    if (-not $SkipXashBuild) {
        $evidence += "``.\waf.bat build --targets=xash`` passed"
    }
    if (-not $SkipFullTests) {
        if (-not [string]::IsNullOrWhiteSpace($FullTestSummary)) {
            $evidence += "``.\waf.bat build --alltests`` $FullTestSummary"
        } else {
            $evidence += "``.\waf.bat build --alltests`` passed"
        }
    }
    if (-not $SkipSmoke -and -not [string]::IsNullOrWhiteSpace($SmokeFirstFrame)) {
        $evidence += ("``{0}`` reached first frame in {1} seconds and stopped with reason ``{2}``" -f
            (Format-CommandLine "run-win32\xash3d.exe" $SmokeArgs),
            $SmokeFirstFrame,
            $SmokeStopReason)
    }

    if ($evidence.Count -gt 0) {
        $lines += ""
        $lines += "## Evidence"
        $lines += ""
        $lines += ("Evidence: {0}." -f ($evidence -join ", "))
    }

    if (-not [string]::IsNullOrWhiteSpace($SmokeLogPath)) {
        $lines += ""
        $lines += "- Smoke log: $SmokeLogPath"
    }

    Write-Host ""
    Write-Host "== Validation Summary =="
    $lines | ForEach-Object { Write-Host $_ }

    if (-not [string]::IsNullOrWhiteSpace($ReportPath)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath) | Out-Null
        $lines | Set-Content -LiteralPath $ReportPath -Encoding ASCII
        Write-Host ""
        Write-Host ("Report written to {0}" -f $ReportPath)
    }
}

Push-Location $RepoRoot
try {
    if (-not $SkipFocused -and -not [string]::IsNullOrWhiteSpace($FocusedTarget)) {
        Invoke-ValidationStep "Focused test target" {
            $output = Invoke-NativeCommand ".\waf.bat" @("build", "--targets=$FocusedTarget")
            $script:FocusedTestSummary = Get-TestPassSummary $output
            $script:FocusedTestSummary
        }
    }

    if (-not $SkipXashBuild) {
        Invoke-ValidationStep "Build xash" {
            Invoke-NativeCommand ".\waf.bat" @("build", "--targets=xash") | Out-Null
            "xash target built"
        }
    }

    if (-not $SkipFullTests) {
        Invoke-ValidationStep "Full test suite" {
            $output = Invoke-NativeCommand ".\waf.bat" @("build", "--alltests")
            $script:FullTestSummary = Get-TestPassSummary $output
            $script:FullTestSummary
        }
    }

    if (-not $SkipSmoke) {
        if (-not $SkipRuntimeCopy) {
            Invoke-ValidationStep "Refresh runtime binaries" {
                Refresh-RuntimeBinaries
            }
        }

        Invoke-ValidationStep "Runtime smoke" {
            Invoke-SmokeTest
        }
    }
} catch {
    $ExitCode = 1
    Write-Host ""
    Write-Host ("Validation failed: {0}" -f $_.Exception.Message)
} finally {
    Pop-Location
    Write-ValidationReport
}

exit $ExitCode
