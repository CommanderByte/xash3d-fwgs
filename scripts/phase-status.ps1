param(
    [int]$PhaseNumber = 0,
    [string]$TasksPath = "Documentation/codex/tasks.md",
    [switch]$AllOpen
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$FullPath = if ([System.IO.Path]::IsPathRooted($TasksPath)) {
    $TasksPath
} else {
    Join-Path $RepoRoot $TasksPath
}

$lines = Get-Content -LiteralPath $FullPath
$phases = @()
$current = $null

foreach ($line in $lines) {
    if ($line -match "^## Phase\s+([0-9]+):\s+(.+)$") {
        if ($current) {
            $phases += $current
        }
        $current = [pscustomobject]@{
            Number = [int]$Matches[1]
            Title = $Matches[2]
            Lines = @($line)
        }
    } elseif ($current) {
        $current.Lines += $line
    }
}

if ($current) {
    $phases += $current
}

if ($PhaseNumber -ne 0) {
    $selected = @($phases | Where-Object { $_.Number -eq $PhaseNumber })
} elseif ($AllOpen) {
    $selected = @($phases | Where-Object { ($_.Lines -join "`n") -match "- \[ \]" })
} else {
    $selected = @($phases | Where-Object { ($_.Lines -join "`n") -match "- \[ \]" } | Select-Object -First 1)
}

if ($selected.Count -eq 0) {
    Write-Host "No matching phase found."
    exit 0
}

foreach ($phase in $selected) {
    $checked = 0
    $open = 0
    $missingEvidence = @()
    $lastTask = ""

    for ($i = 0; $i -lt $phase.Lines.Count; ++$i) {
        $line = $phase.Lines[$i]
        if ($line -match '^- \[x\]\s+`([^`]+)`') {
            ++$checked
            $lastTask = $Matches[1]
        } elseif ($line -match '^- \[ \]\s+`([^`]+)`') {
            ++$open
            $lastTask = $Matches[1]
        } elseif ($line -match "^\s*Evidence:\s*$" -and -not [string]::IsNullOrWhiteSpace($lastTask)) {
            $missingEvidence += $lastTask
        }
    }

    Write-Host ("Phase {0}: {1}" -f $phase.Number, $phase.Title)
    Write-Host ("  Done: {0}  Open: {1}" -f $checked, $open)

    $openLines = @($phase.Lines | Where-Object { $_ -match "^- \[ \]" })
    if ($openLines.Count -gt 0) {
        Write-Host "  Open items:"
        foreach ($item in $openLines) {
            Write-Host ("    {0}" -f $item)
        }
    }

    if ($missingEvidence.Count -gt 0) {
        Write-Host ("  Missing evidence: {0}" -f ($missingEvidence -join ", "))
    }

    Write-Host ""
}
