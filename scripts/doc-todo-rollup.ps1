param(
    [string[]]$Paths = @(
        "Documentation/codex/tasks.md",
        "Documentation/codex/todo"
    ),

    [switch]$ShowOpen
)

$ErrorActionPreference = "Stop"

function Get-MarkdownFiles {
    param([string[]]$InputPaths)

    foreach ($path in $InputPaths) {
        if (-not (Test-Path -LiteralPath $path)) {
            Write-Warning "Skipping missing path: $path"
            continue
        }

        $item = Get-Item -LiteralPath $path
        if ($item.PSIsContainer) {
            Get-ChildItem -LiteralPath $item.FullName -Recurse -File -Filter *.md
        } else {
            $item
        }
    }
}

$files = @(Get-MarkdownFiles -InputPaths $Paths | Sort-Object FullName -Unique)
$rows = @()
$openLines = @()

foreach ($file in $files) {
    $open = 0
    $done = 0
    $lineNumber = 0

    foreach ($line in [System.IO.File]::ReadLines($file.FullName)) {
        $lineNumber++

        if ($line -match '- \[ \]') {
            $open++
            if ($ShowOpen) {
                $openLines += [pscustomobject]@{
                    File = $file.FullName
                    Line = $lineNumber
                    Text = $line.Trim()
                }
            }
        } elseif ($line -match '- \[[xX]\]') {
            $done++
        }
    }

    if ($open -gt 0 -or $done -gt 0) {
        $rows += [pscustomobject]@{
            File = Resolve-Path -Relative $file.FullName
            Open = $open
            Done = $done
            Total = $open + $done
        }
    }
}

if ($rows.Count -eq 0) {
    Write-Output "No Markdown task checkboxes found."
    exit 0
}

$rows |
    Sort-Object -Property @{ Expression = "Open"; Descending = $true }, "File" |
    Format-Table -AutoSize

if ($ShowOpen -and $openLines.Count -gt 0) {
    Write-Output ""
    Write-Output "Open items:"
    foreach ($item in $openLines) {
        $relative = Resolve-Path -Relative $item.File
        Write-Output ("{0}:{1}: {2}" -f $relative, $item.Line, $item.Text)
    }
}
