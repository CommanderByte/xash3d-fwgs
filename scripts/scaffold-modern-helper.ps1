param(
    [Parameter(Mandatory = $true)]
    [string]$Module,

    [Parameter(Mandatory = $true)]
    [string]$Name,

    [string]$TestGroup = "",
    [string]$Namespace = "",
    [string]$AdapterDir = "",
    [string]$AdapterName = "",
    [switch]$NoAdapter,
    [switch]$NoWscript,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Normalize-RepoPath {
    param([string]$Path)
    return ($Path -replace "\\", "/").Trim("/")
}

function To-Identifier {
    param([string]$Value)
    return (($Value -replace "[^A-Za-z0-9]+", "_").Trim("_")).ToLowerInvariant()
}

function To-IncludeGuard {
    param([string]$Path)
    $normalizedPath = Normalize-RepoPath $Path
    $normalizedPath = $normalizedPath -replace "^src/include/", ""
    return ("XASH_{0}" -f ($normalizedPath -replace "[^A-Za-z0-9]+", "_")).ToUpperInvariant()
}

function Get-NamespaceParts {
    param(
        [string]$NamespaceValue,
        [string]$ModuleValue
    )

    if (-not [string]::IsNullOrWhiteSpace($NamespaceValue)) {
        return @($NamespaceValue -split "::" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }

    $normalizedModule = Normalize-RepoPath $ModuleValue
    $parts = @("xash")
    $parts += @($normalizedModule -split "/" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    return $parts
}

function Open-Namespace {
    param([string[]]$Parts)

    $lines = @()
    foreach ($part in $Parts) {
        $lines += "namespace $part"
        $lines += "{"
    }

    return $lines
}

function Close-Namespace {
    param([string[]]$Parts)

    $lines = @()
    for ($i = $Parts.Count - 1; $i -ge 0; --$i) {
        $lines += "}"
    }

    return $lines
}

function Write-NewFile {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $fullPath = Join-Path $RepoRoot $Path
    if ((Test-Path -LiteralPath $fullPath) -and -not $Force) {
        throw "Refusing to overwrite existing file: $Path"
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fullPath) | Out-Null
    $Lines | Set-Content -LiteralPath $fullPath -Encoding ASCII
    Write-Host "created $Path"
}

function Insert-TestTarget {
    param(
        [string]$TargetName,
        [string]$TestPath
    )

    $wscript = Join-Path $RepoRoot "src/wscript"
    $content = Get-Content -LiteralPath $wscript
    if (($content -join "`n") -match [regex]::Escape("'$TargetName'")) {
        Write-Host "src/wscript already contains test target $TargetName"
        return
    }

    $start = -1
    for ($i = 0; $i -lt $content.Count; ++$i) {
        if ($content[$i] -match "^\s*tests\s*=\s*\{") {
            $start = $i
            break
        }
    }

    if ($start -lt 0) {
        Write-Warning "Could not find tests dictionary in src/wscript; add $TargetName manually."
        return
    }

    $insert = -1
    for ($i = $start + 1; $i -lt $content.Count; ++$i) {
        if ($content[$i] -match "^\s*\}\s*$") {
            $insert = $i
            break
        }
    }

    if ($insert -lt 0) {
        Write-Warning "Could not find tests dictionary end in src/wscript; add $TargetName manually."
        return
    }

    $line = "`t`t`t'$TargetName': '../$TestPath',"
    $updated = @()
    $updated += $content[0..($insert - 1)]
    $updated += $line
    $updated += $content[$insert..($content.Count - 1)]
    $updated | Set-Content -LiteralPath $wscript -Encoding ASCII
    Write-Host "wired src/wscript test target $TargetName"
}

function Insert-EngineAdapter {
    param([string]$AdapterSource)

    $wscript = Join-Path $RepoRoot "engine/wscript"
    $content = Get-Content -LiteralPath $wscript
    if (($content -join "`n") -match [regex]::Escape("'$AdapterSource'")) {
        Write-Host "engine/wscript already contains adapter $AdapterSource"
        return
    }

    $insert = -1
    for ($i = 0; $i -lt $content.Count; ++$i) {
        if ($content[$i] -match "'server/.*_adapter\.cpp'") {
            $insert = $i + 1
        }
    }

    if ($insert -lt 0) {
        Write-Warning "Could not find server adapter list in engine/wscript; add $AdapterSource manually."
        return
    }

    $line = "`t`t'$AdapterSource',"
    $updated = @()
    $updated += $content[0..($insert - 1)]
    $updated += $line
    $updated += $content[$insert..($content.Count - 1)]
    $updated | Set-Content -LiteralPath $wscript -Encoding ASCII
    Write-Host "wired engine/wscript adapter $AdapterSource"
}

$modulePath = Normalize-RepoPath $Module
$nameId = To-Identifier $Name
if ([string]::IsNullOrWhiteSpace($TestGroup)) {
    $TestGroup = ($modulePath -split "/")[0]
}

$testGroupPath = Normalize-RepoPath $TestGroup
$namespaceParts = Get-NamespaceParts -NamespaceValue $Namespace -ModuleValue $modulePath

$headerPath = "src/include/$modulePath/$nameId.hpp"
$sourcePath = "src/$modulePath/$nameId.cpp"
$testPath = "tests/$testGroupPath/$nameId.cpp"
$includePath = "$modulePath/$nameId.hpp"
$testTarget = "{0}_{1}" -f (To-Identifier $TestGroup), $nameId

$headerGuard = To-IncludeGuard $headerPath
$headerLines = @()
$headerLines += "#ifndef $headerGuard"
$headerLines += "#define $headerGuard"
$headerLines += ""
$headerLines += Open-Namespace $namespaceParts
$headerLines += ""
$headerLines += "// Add target-neutral declarations here."
$headerLines += ""
$headerLines += Close-Namespace $namespaceParts
$headerLines += ""
$headerLines += "#endif"

$sourceLines = @()
$sourceLines += "#include `"$includePath`""
$sourceLines += ""
$sourceLines += Open-Namespace $namespaceParts
$sourceLines += ""
$sourceLines += "// Add target-neutral implementation here."
$sourceLines += ""
$sourceLines += Close-Namespace $namespaceParts

$testLines = @()
$testLines += "#include <cstdlib>"
$testLines += ""
$testLines += "#include `"$includePath`""
$testLines += ""
$testLines += "int main()"
$testLines += "{"
$testLines += "`treturn EXIT_SUCCESS;"
$testLines += "}"

Write-NewFile -Path $headerPath -Lines $headerLines
Write-NewFile -Path $sourcePath -Lines $sourceLines
Write-NewFile -Path $testPath -Lines $testLines

if (-not $NoAdapter -and -not [string]::IsNullOrWhiteSpace($AdapterDir)) {
    $adapterDirPath = Normalize-RepoPath $AdapterDir
    if ([string]::IsNullOrWhiteSpace($AdapterName)) {
        $AdapterName = "${nameId}_adapter"
    }

    $adapterNameId = To-Identifier $AdapterName
    $adapterHeaderPath = "$adapterDirPath/$adapterNameId.h"
    $adapterSourcePath = "$adapterDirPath/$adapterNameId.cpp"
    $adapterGuard = To-IncludeGuard $adapterHeaderPath

    $adapterHeaderLines = @()
    $adapterHeaderLines += "#ifndef $adapterGuard"
    $adapterHeaderLines += "#define $adapterGuard"
    $adapterHeaderLines += ""
    $adapterHeaderLines += "#ifdef __cplusplus"
    $adapterHeaderLines += "extern `"C`" {"
    $adapterHeaderLines += "#endif"
    $adapterHeaderLines += ""
    $adapterHeaderLines += "/* Add C-compatible adapter declarations here. */"
    $adapterHeaderLines += ""
    $adapterHeaderLines += "#ifdef __cplusplus"
    $adapterHeaderLines += "}"
    $adapterHeaderLines += "#endif"
    $adapterHeaderLines += ""
    $adapterHeaderLines += "#endif"

    $adapterSourceLines = @()
    $adapterSourceLines += "#include `"$adapterNameId.h`""
    $adapterSourceLines += ""
    $adapterSourceLines += "#include `"$includePath`""
    $adapterSourceLines += ""
    $adapterSourceLines += "/* Add legacy-to-modern conversions here. */"

    Write-NewFile -Path $adapterHeaderPath -Lines $adapterHeaderLines
    Write-NewFile -Path $adapterSourcePath -Lines $adapterSourceLines

    if (-not $NoWscript -and $adapterDirPath.StartsWith("engine/")) {
        $relativeAdapterSource = (Normalize-RepoPath $adapterSourcePath).Substring("engine/".Length)
        Insert-EngineAdapter -AdapterSource $relativeAdapterSource
    }
}

if (-not $NoWscript) {
    Insert-TestTarget -TargetName $testTarget -TestPath $testPath
}

Write-Host ""
Write-Host "Next:"
Write-Host "  1. Fill in the helper, adapter, and tests."
Write-Host "  2. Run .\waf.bat build --targets=test_$testTarget."
Write-Host "  3. Use scripts/run-phase-validation.ps1 once the adapter is live."
