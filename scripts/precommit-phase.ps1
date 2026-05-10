param(
    [string]$FocusedTarget = "",
    [switch]$SkipValidation,
    [switch]$SkipFullTests,
    [switch]$SkipSmoke,
    [switch]$StopRunningXash,
    [switch]$NoReportFile
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ValidationScript = Join-Path $RepoRoot "scripts/run-phase-validation.ps1"

Push-Location $RepoRoot
try {
    Write-Host "== Git Status =="
    git status --short

    Write-Host ""
    Write-Host "== Diff Check =="
    git diff --check
    if ($LASTEXITCODE -ne 0) {
        throw "git diff --check failed"
    }

    if (-not $SkipValidation) {
        $args = @()
        if (-not [string]::IsNullOrWhiteSpace($FocusedTarget)) {
            $args += "-FocusedTarget"
            $args += $FocusedTarget
        }
        if ($SkipFullTests) {
            $args += "-SkipFullTests"
        }
        if ($SkipSmoke) {
            $args += "-SkipSmoke"
        }
        if ($StopRunningXash) {
            $args += "-StopRunningXash"
        }
        if ($NoReportFile) {
            $args += "-NoReportFile"
        }

        Write-Host ""
        Write-Host "== Phase Validation =="
        & $ValidationScript @args
        if ($LASTEXITCODE -ne 0) {
            throw "phase validation failed"
        }
    }

    Write-Host ""
    Write-Host "Precommit phase checks passed. Review the diff, then commit manually."
} finally {
    Pop-Location
}
