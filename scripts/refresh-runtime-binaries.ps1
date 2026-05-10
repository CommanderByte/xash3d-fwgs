param(
    [string]$RuntimeDir = "run-win32",
    [string]$BuildRoot = "build",
    [switch]$CopyLauncher,
    [switch]$DryRun,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return (Join-Path (Get-Location) $Path)
}

function Find-FirstExisting {
    param([string[]]$Sources)

    foreach ($source in $Sources) {
        $path = Resolve-RepoPath $source
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    return $null
}

$runtimePath = Resolve-RepoPath $RuntimeDir
if (-not (Test-Path -LiteralPath $runtimePath)) {
    throw "Runtime directory not found: $runtimePath"
}

$pairs = @(
    @{ Destination = "xash.dll"; Sources = @("$BuildRoot\engine\xash.dll") },
    @{ Destination = "xash.pdb"; Sources = @("$BuildRoot\engine\xash.pdb") },
    @{ Destination = "filesystem_stdio.dll"; Sources = @("$BuildRoot\filesystem\filesystem_stdio.dll") },
    @{ Destination = "filesystem_stdio.pdb"; Sources = @("$BuildRoot\filesystem\filesystem_stdio.pdb") },
    @{ Destination = "ref_gl.dll"; Sources = @("$BuildRoot\ref\gl\ref_gl.dll") },
    @{ Destination = "ref_gl.pdb"; Sources = @("$BuildRoot\ref\gl\ref_gl.pdb") }
)

if ($CopyLauncher) {
    $pairs = @(
        @{ Destination = "xash3d.exe"; Sources = @("$BuildRoot\src\xash3d.exe", "$BuildRoot\game_launch\xash3d.exe") },
        @{ Destination = "xash3d.pdb"; Sources = @("$BuildRoot\src\xash3d.pdb", "$BuildRoot\game_launch\xash3d.pdb") }
    ) + $pairs
}

$copied = 0
$skipped = 0
$missing = 0

foreach ($pair in $pairs) {
    $source = Find-FirstExisting -Sources $pair.Sources
    $destination = Join-Path $runtimePath $pair.Destination

    if ($null -eq $source) {
        $missing++
        Write-Output ("missing: {0}" -f $pair.Destination)
        continue
    }

    $sourceInfo = Get-Item -LiteralPath $source
    $shouldCopy = $Force -or -not (Test-Path -LiteralPath $destination)

    if (-not $shouldCopy) {
        $destinationInfo = Get-Item -LiteralPath $destination
        $shouldCopy = $sourceInfo.LastWriteTime -gt $destinationInfo.LastWriteTime
    }

    if (-not $shouldCopy) {
        $skipped++
        Write-Output ("current: {0}" -f $pair.Destination)
        continue
    }

    if ($DryRun) {
        Write-Output ("would copy: {0} -> {1}" -f (Resolve-Path -Relative $source), (Resolve-Path -Relative $destination))
    } else {
        Copy-Item -LiteralPath $source -Destination $destination -Force
        Write-Output ("copied: {0}" -f $pair.Destination)
    }

    $copied++
}

Write-Output ""
Write-Output ("summary: copied={0}, current={1}, missing={2}, dryRun={3}" -f $copied, $skipped, $missing, [bool]$DryRun)
