param(
    [string]$Python = "python",
    [string]$BaseUrl = $env:LMSTUDIO_BASE_URL,
    [string]$Model = $env:LMSTUDIO_MODEL,
    [switch]$AskSmoke
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BaseUrl)) {
    $BaseUrl = "http://localhost:1234/v1"
}

$serverPath = Join-Path (Get-Location) "scripts/lmstudio-mcp-server.py"
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $Python
$startInfo.Arguments = '"' + $serverPath + '"'
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.UseShellExecute = $false
$startInfo.WorkingDirectory = (Get-Location).Path

$process = [System.Diagnostics.Process]::Start($startInfo)

function Send-Mcp {
    param([hashtable]$Message)

    $json = $Message | ConvertTo-Json -Depth 20 -Compress
    $process.StandardInput.WriteLine($json)
    $process.StandardInput.Flush()
    $line = $process.StandardOutput.ReadLine()
    if ([string]::IsNullOrWhiteSpace($line)) {
        throw "MCP server returned no response."
    }

    return $line | ConvertFrom-Json
}

try {
    $init = Send-Mcp @{
        jsonrpc = "2.0"
        id = 1
        method = "initialize"
        params = @{
            protocolVersion = "2025-06-18"
            capabilities = @{}
            clientInfo = @{ name = "xash-smoke"; version = "0.1.0" }
        }
    }

    $tools = Send-Mcp @{
        jsonrpc = "2.0"
        id = 2
        method = "tools/list"
        params = @{}
    }

    $status = Send-Mcp @{
        jsonrpc = "2.0"
        id = 3
        method = "tools/call"
        params = @{
            name = "lmstudio_status"
            arguments = @{ base_url = $BaseUrl }
        }
    }

    $serverName = "xash-lmstudio-bridge"
    $serverVersion = "0.1.0"
    if ($init.result -and $init.result.serverInfo) {
        $serverNameProperty = $init.result.serverInfo.PSObject.Properties | Where-Object { $_.Name -eq "name" } | Select-Object -First 1
        $serverVersionProperty = $init.result.serverInfo.PSObject.Properties | Where-Object { $_.Name -eq "version" } | Select-Object -First 1
        if ($serverNameProperty) {
            $serverName = $serverNameProperty.Value
        }
        if ($serverVersionProperty) {
            $serverVersion = $serverVersionProperty.Value
        }
    }
    Write-Output ("server: {0} {1}" -f $serverName, $serverVersion)
    Write-Output "tools:"
    $tools.result.tools | ForEach-Object { Write-Output ("  - {0}" -f $_.name) }
    Write-Output ""
    Write-Output "status:"
    Write-Output $status.result.content[0].text

    if ($AskSmoke) {
        $arguments = @{
            base_url = $BaseUrl
            prompt = "Reply with OK and one short sentence confirming the MCP ask tool works."
            max_tokens = 80
        }
        if (-not [string]::IsNullOrWhiteSpace($Model)) {
            $arguments.model = $Model
        }

        $ask = Send-Mcp @{
            jsonrpc = "2.0"
            id = 4
            method = "tools/call"
            params = @{
                name = "ask_lmstudio"
                arguments = $arguments
            }
        }

        Write-Output ""
        Write-Output "ask smoke:"
        Write-Output $ask.result.content[0].text
    }
}
finally {
    if (-not $process.HasExited) {
        $process.Kill()
    }
}
