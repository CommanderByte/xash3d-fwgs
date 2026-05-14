[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$BaseUrl = $env:LMSTUDIO_BASE_URL,
    [string]$Model = $env:LMSTUDIO_MODEL,
    [string]$ApiKey = $env:LMSTUDIO_API_KEY,
    [string]$Prompt = "",
    [string]$PromptFile = "",
    [string[]]$Files = @(),
    [int]$MaxFileChars = 12000,
    [int]$MaxTokens = 1024,
    [double]$Temperature = 0.2,
    [string]$OutputPath = ".codex-cache/local-agent/last-response.md",
    [string]$LogPrefix = "",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraFiles = @()
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

if ([string]::IsNullOrWhiteSpace($LogPrefix)) {
    $leaf = [System.IO.Path]::GetFileNameWithoutExtension($OutputPath)
    if ([string]::IsNullOrWhiteSpace($leaf)) {
        $leaf = "last-response"
    }

    $LogPrefix = Join-Path ".codex-cache/local-agent" $leaf
}

$logDirectory = Split-Path -Parent $LogPrefix
if (-not [string]::IsNullOrWhiteSpace($logDirectory)) {
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
}

if ([string]::IsNullOrWhiteSpace($Prompt) -and [string]::IsNullOrWhiteSpace($PromptFile)) {
    if ([Console]::IsInputRedirected) {
        $Prompt = [Console]::In.ReadToEnd()
    }
}

if (-not [string]::IsNullOrWhiteSpace($Prompt) -and [string]::IsNullOrWhiteSpace($PromptFile)) {
    $PromptFile = "$LogPrefix.prompt.txt"
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText((Join-Path (Get-Location) $PromptFile), $Prompt, $encoding)
}

if ([string]::IsNullOrWhiteSpace($PromptFile)) {
    throw "Provide -Prompt, -PromptFile, or pipe prompt text on stdin."
}

if ($ExtraFiles.Count -gt 0) {
    $Files += $ExtraFiles
}

$requestPath = "$LogPrefix.request.json"
$runnerPath = "$LogPrefix.run.ps1"
$stdoutPath = "$LogPrefix.stdout.log"
$stderrPath = "$LogPrefix.stderr.log"
$statusPath = "$LogPrefix.started.json"

$request = [ordered]@{
    baseUrl = $BaseUrl
    model = $Model
    apiKey = $ApiKey
    promptFile = $PromptFile
    files = @($Files)
    maxFileChars = $MaxFileChars
    maxTokens = $MaxTokens
    temperature = $Temperature
    outputPath = $OutputPath
}

$request | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $requestPath -Encoding UTF8

$askScript = Join-Path $repoRoot "scripts/ask-local-model.ps1"
$escapedRequestPath = (Join-Path (Get-Location) $requestPath).Replace("'", "''")
$escapedAskScript = $askScript.Replace("'", "''")

$runner = @"
`$ErrorActionPreference = 'Stop'
`$ProgressPreference = 'SilentlyContinue'
`$request = Get-Content -LiteralPath '$escapedRequestPath' -Raw | ConvertFrom-Json
`$files = @()
foreach (`$file in @(`$request.files)) {
    if (-not [string]::IsNullOrWhiteSpace(`$file)) {
        `$files += [string]`$file
    }
}

`$params = @{
    PromptFile = [string]`$request.promptFile
    Files = `$files
    MaxFileChars = [int]`$request.maxFileChars
    MaxTokens = [int]`$request.maxTokens
    Temperature = [double]`$request.temperature
    OutputPath = [string]`$request.outputPath
}

if (-not [string]::IsNullOrWhiteSpace(`$request.baseUrl)) {
    `$params.BaseUrl = [string]`$request.baseUrl
}
if (-not [string]::IsNullOrWhiteSpace(`$request.model)) {
    `$params.Model = [string]`$request.model
}
if (-not [string]::IsNullOrWhiteSpace(`$request.apiKey)) {
    `$params.ApiKey = [string]`$request.apiKey
}

& '$escapedAskScript' @params
"@

$encoding = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Join-Path (Get-Location) $runnerPath), $runner, $encoding)

$process = Start-Process -FilePath powershell `
    -WindowStyle Hidden `
    -PassThru `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -ArgumentList @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        (Join-Path (Get-Location) $runnerPath)
    )

$status = [ordered]@{
    processId = $process.Id
    startedAt = (Get-Date).ToString("o")
    outputPath = $OutputPath
    stdoutPath = $stdoutPath
    stderrPath = $stderrPath
    requestPath = $requestPath
    runnerPath = $runnerPath
    files = @($Files)
}

$status | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statusPath -Encoding UTF8

Write-Output "Started LM Studio request PID $($process.Id)"
Write-Output "Output: $OutputPath"
Write-Output "Stdout: $stdoutPath"
Write-Output "Stderr: $stderrPath"
Write-Output "Status: $statusPath"
