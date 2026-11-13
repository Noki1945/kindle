$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$outputDirectory = Join-Path $projectRoot 'runtime\device-baseline'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$snapshot = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot 'scripts\kindle-ssh.ps1') 'date; cat /proc/uptime; cat /proc/loadavg; cat /proc/meminfo; ps -eo pid,comm,rss --sort=-rss; cat /proc/stat'
if ($LASTEXITCODE) { throw 'Device baseline capture failed' }
$outputPath = Join-Path $outputDirectory "$timestamp.txt"
[IO.File]::WriteAllLines($outputPath, [string[]]$snapshot, (New-Object Text.UTF8Encoding($false)))
Write-Output "Saved $outputPath"
