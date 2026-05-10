param(
    [string]$LegacyDir = "engine/server",
    [string]$ModernDir = "src/engine/server",
    [string[]]$Extensions = @(".c", ".cpp", ".h", ".hpp"),
    [int]$Top = 80
)

$ErrorActionPreference = "Stop"

function Get-CodeFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,

        [Parameter(Mandatory = $true)]
        [string[]]$AllowedExtensions
    )

    if (-not (Test-Path -LiteralPath $Root)) {
        return @()
    }

    @(Get-ChildItem -LiteralPath $Root -Recurse -File | Where-Object {
        $AllowedExtensions -contains $_.Extension.ToLowerInvariant()
    })
}

$legacyFiles = @(Get-CodeFiles -Root $LegacyDir -AllowedExtensions $Extensions)
$modernFiles = @(Get-CodeFiles -Root $ModernDir -AllowedExtensions $Extensions)
$modernBaseNames = @{}

foreach ($file in $modernFiles) {
    $modernBaseNames[$file.BaseName.ToLowerInvariant()] = $true
}

Write-Output "== Modernization Scan =="
Write-Output "legacy: $LegacyDir"
Write-Output "modern: $ModernDir"
Write-Output ""

Write-Output "Legacy file counts:"
if ($legacyFiles.Count -eq 0) {
    Write-Output "  none"
} else {
    $legacyFiles |
        Group-Object Extension |
        Sort-Object Name |
        ForEach-Object { Write-Output ("  {0}: {1}" -f $_.Name, $_.Count) }
}

Write-Output ""
Write-Output "Modern file counts:"
if ($modernFiles.Count -eq 0) {
    Write-Output "  none"
} else {
    $modernFiles |
        Group-Object Extension |
        Sort-Object Name |
        ForEach-Object { Write-Output ("  {0}: {1}" -f $_.Name, $_.Count) }
}

$legacyOnly = @($legacyFiles | Where-Object {
    -not $modernBaseNames.ContainsKey($_.BaseName.ToLowerInvariant()) -and
    $_.BaseName -notmatch '_adapter$'
} | Sort-Object FullName)

Write-Output ""
Write-Output "Likely legacy-only files by basename:"
if ($legacyOnly.Count -eq 0) {
    Write-Output "  none"
} else {
    $legacyOnly |
        Select-Object -First $Top |
        ForEach-Object { Write-Output ("  {0}" -f (Resolve-Path -Relative $_.FullName)) }

    if ($legacyOnly.Count -gt $Top) {
        Write-Output ("  ... {0} more" -f ($legacyOnly.Count - $Top))
    }
}

Write-Output ""
Write-Output "Note: this is a triage helper. Basename matches are hints, not migration proof."
