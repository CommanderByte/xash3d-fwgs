param(
    [Parameter(Mandatory = $true)]
    [int]$PhaseNumber,

    [Parameter(Mandatory = $true)]
    [string]$Title,

    [Parameter(Mandatory = $true)]
    [string]$Prefix,

    [string]$TasksPath = "Documentation/codex/tasks.md",
    [string]$TodoPath = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $RepoRoot $Path
}

function New-TaskBlock {
    param(
        [int]$Number,
        [string]$PhaseTitle,
        [string]$TaskPrefix
    )

    $items = @(
        "Baseline the relevant legacy behavior and compatibility boundaries.",
        "Implement the target-neutral helper or planner.",
        "Add focused tests for the preserved behavior and edge cases.",
        "Route legacy code through the helper while keeping side effects legacy-owned.",
        "Run focused validation, full tests, smoke, and record first-frame timing."
    )

    $lines = @()
    $lines += "## Phase ${Number}: $PhaseTitle"
    $lines += ""

    for ($i = 0; $i -lt $items.Count; ++$i) {
        $id = "{0}-{1:000}" -f $TaskPrefix, ($i + 1)
        $lines += "- [ ] ``$id`` $($items[$i])"
        $lines += "  Evidence:"
    }

    return $lines
}

function Insert-PhaseBlock {
    param(
        [string]$Path,
        [string[]]$Block
    )

    $fullPath = Resolve-RepoPath $Path
    $content = Get-Content -LiteralPath $fullPath
    $phaseHeading = "## Phase ${PhaseNumber}:"
    if (($content -join "`n") -match [regex]::Escape($phaseHeading) -and -not $Force) {
        throw "Phase $PhaseNumber already exists in $Path"
    }

    $insert = -1
    for ($i = 0; $i -lt $content.Count; ++$i) {
        if ($content[$i] -match "^## Phase\s+([0-9]+):") {
            $existing = [int]$Matches[1]
            if ($existing -gt $PhaseNumber) {
                $insert = $i
                break
            }
        }

        if ($content[$i] -match "^## Decision Log" -and $insert -lt 0) {
            $insert = $i
            break
        }
    }

    if ($insert -lt 0) {
        $insert = $content.Count
    }

    $updated = @()
    if ($insert -gt 0) {
        $updated += $content[0..($insert - 1)]
        if ($updated[-1] -ne "") {
            $updated += ""
        }
    }
    $updated += $Block
    $updated += ""
    if ($insert -lt $content.Count) {
        $updated += $content[$insert..($content.Count - 1)]
    }

    $updated | Set-Content -LiteralPath $fullPath -Encoding ASCII
    Write-Host "inserted Phase $PhaseNumber into $Path"
}

$taskBlock = New-TaskBlock -Number $PhaseNumber -PhaseTitle $Title -TaskPrefix $Prefix
Insert-PhaseBlock -Path $TasksPath -Block $taskBlock

if (-not [string]::IsNullOrWhiteSpace($TodoPath)) {
    $todoFullPath = Resolve-RepoPath $TodoPath
    if (-not (Test-Path -LiteralPath $todoFullPath)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $todoFullPath) | Out-Null
        @("# $Title", "") | Set-Content -LiteralPath $todoFullPath -Encoding ASCII
    }

    $todoLines = @()
    $todoLines += "## Phase ${PhaseNumber}: $Title"
    $todoLines += ""
    $todoLines += "Goal: describe the compatibility-preserving migration slice before implementation."
    $todoLines += ""
    $todoLines += "- [ ] Baseline legacy behavior."
    $todoLines += "- [ ] Implement target-neutral code."
    $todoLines += "- [ ] Add focused tests."
    $todoLines += "- [ ] Route through a legacy adapter when needed."
    $todoLines += "- [ ] Run validation and record evidence."
    $todoLines += ""

    Add-Content -LiteralPath $todoFullPath -Value $todoLines -Encoding ASCII
    Write-Host "appended Phase $PhaseNumber to $TodoPath"
}
