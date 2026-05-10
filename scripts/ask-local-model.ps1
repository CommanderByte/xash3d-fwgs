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
    [string]$OutputPath = ".codex-cache/local-agent/last-response.md"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BaseUrl)) {
    $BaseUrl = "http://localhost:1234/v1"
}

if ([string]::IsNullOrWhiteSpace($ApiKey)) {
    $ApiKey = "lm-studio"
}

$BaseUrl = $BaseUrl.TrimEnd("/")

function Get-LoadedModel {
    param([string]$Url)

    $models = Invoke-RestMethod -Method Get -Uri "$Url/models" -Headers @{
        Authorization = "Bearer $ApiKey"
    } -TimeoutSec 15

    $loaded = @($models.data)
    if ($loaded.Count -eq 0) {
        throw "LM Studio responded, but no loaded models were reported by $Url/models."
    }

    return $loaded[0].id
}

function Read-Excerpt {
    param(
        [string]$Path,
        [int]$Limit
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "File not found: $Path"
    }

    $content = Get-Content -LiteralPath $Path -Raw
    if ($content.Length -le $Limit) {
        return $content
    }

    return $content.Substring(0, $Limit) + "`n...[truncated after $Limit chars]..."
}

if (-not [string]::IsNullOrWhiteSpace($PromptFile)) {
    $Prompt = Get-Content -LiteralPath $PromptFile -Raw
}

if ([string]::IsNullOrWhiteSpace($Prompt)) {
    $stdin = [Console]::In.ReadToEnd()
    if (-not [string]::IsNullOrWhiteSpace($stdin)) {
        $Prompt = $stdin
    }
}

if ([string]::IsNullOrWhiteSpace($Prompt)) {
    throw "Provide -Prompt, -PromptFile, or pipe prompt text on stdin."
}

if ([string]::IsNullOrWhiteSpace($Model)) {
    $Model = Get-LoadedModel -Url $BaseUrl
}

$userContent = New-Object System.Text.StringBuilder
[void]$userContent.AppendLine($Prompt.Trim())

foreach ($file in $Files) {
    $relative = Resolve-Path -Relative $file
    [void]$userContent.AppendLine("")
    [void]$userContent.AppendLine("## File: $relative")
    [void]$userContent.AppendLine('```')
    [void]$userContent.AppendLine((Read-Excerpt -Path $file -Limit $MaxFileChars))
    [void]$userContent.AppendLine('```')
}

$body = @{
    model = $Model
    temperature = $Temperature
    max_tokens = $MaxTokens
    messages = @(
        @{
            role = "system"
            content = "You are a read-only local analysis subagent for the Xash3D modernization repo. Summarize findings, suggest tests, and flag uncertainty. Do not claim files were edited."
        },
        @{
            role = "user"
            content = $userContent.ToString()
        }
    )
} | ConvertTo-Json -Depth 20

$response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/chat/completions" -Headers @{
    Authorization = "Bearer $ApiKey"
    "Content-Type" = "application/json"
} -Body $body -TimeoutSec 300

$choice = $response.choices[0]
$text = $choice.message.content
if ([string]::IsNullOrWhiteSpace($text) -and $choice.message.reasoning_content) {
    $text = $choice.message.reasoning_content
}
if ([string]::IsNullOrWhiteSpace($text) -and $choice.text) {
    $text = $choice.text
}
if ([string]::IsNullOrWhiteSpace($text)) {
    New-Item -ItemType Directory -Force -Path ".codex-cache/local-agent" | Out-Null
    $response | ConvertTo-Json -Depth 20 |
        Set-Content -LiteralPath ".codex-cache/local-agent/last-empty-response.json"
    throw "LM Studio returned an empty response. Raw response written to .codex-cache/local-agent/last-empty-response.json."
}

$outputDirectory = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$encoding = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Join-Path (Get-Location) $OutputPath), $text, $encoding)

Write-Output $text
Write-Output ""
Write-Output "Report written to $OutputPath"
