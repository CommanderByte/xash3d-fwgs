param(
    [Parameter(Mandatory = $true)]
    [string]$TaskId,

    [Parameter(Mandatory = $true)]
    [string]$Evidence,

    [string]$TasksPath = "Documentation/codex/tasks.md",

    [switch]$MarkDone,
    [switch]$AllowDuplicate
)

$ErrorActionPreference = "Stop"

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string[]]$Lines
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllLines((Resolve-Path $Path), $Lines, $encoding)
}

if (-not (Test-Path -LiteralPath $TasksPath)) {
    throw "Tasks file not found: $TasksPath"
}

$resolvedPath = (Resolve-Path $TasksPath).Path
$lines = New-Object System.Collections.Generic.List[string]
[System.IO.File]::ReadAllLines($resolvedPath) | ForEach-Object {
    [void]$lines.Add($_)
}

$taskPattern = '^- \[[ xX]\] `' + [regex]::Escape($TaskId) + '`'
$taskIndex = -1

for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match $taskPattern) {
        $taskIndex = $i
        break
    }
}

if ($taskIndex -lt 0) {
    throw "Task id not found in ${TasksPath}: $TaskId"
}

$evidenceLine = "  Evidence: $Evidence"

if (-not $AllowDuplicate) {
    foreach ($line in $lines) {
        if ($line -eq $evidenceLine) {
            Write-Output "Evidence already present for ${TaskId}; no change made."
            exit 0
        }
    }
}

if ($MarkDone) {
    $lines[$taskIndex] = $lines[$taskIndex] -replace '^- \[ \]', '- [x]'
}

$insertIndex = $taskIndex + 1
while ($insertIndex -lt $lines.Count) {
    if ($lines[$insertIndex] -match '^- \[[ xX]\] `' -or $lines[$insertIndex] -match '^## ') {
        break
    }

    $insertIndex++
}

$lines.Insert($insertIndex, $evidenceLine)
Write-Utf8NoBom -Path $resolvedPath -Lines $lines.ToArray()

if ($MarkDone) {
    Write-Output "Marked $TaskId done and appended evidence."
} else {
    Write-Output "Appended evidence for $TaskId."
}
