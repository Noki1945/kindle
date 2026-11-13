param([int]$Seconds = 15, [string]$Output = 'runtime/measurements.csv')
$ErrorActionPreference = 'Stop'
if ($Seconds -lt 1 -or $Seconds -gt 3600) { throw 'Seconds must be 1..3600' }
$targetProcess = Get-Process kindle_sim -ErrorAction Stop
if (@($targetProcess).Count -ne 1) { throw 'Keep exactly one simulator running.' }
$measurements = for ($i = 0; $i -lt $Seconds; $i++) {
    $targetProcess.Refresh()
    [pscustomobject]@{ timestamp = (Get-Date).ToUniversalTime().ToString('o'); platform = 'Windows simulator'; pid = $targetProcess.Id; working_set_bytes = $targetProcess.WorkingSet64; private_bytes = $targetProcess.PrivateMemorySize64; cpu_seconds_total = $targetProcess.TotalProcessorTime.TotalSeconds }
    Start-Sleep -Seconds 1
}
$parent = Split-Path $Output -Parent
if ($parent) { New-Item -ItemType Directory -Force $parent | Out-Null }
$measurements | Export-Csv -NoTypeInformation -Encoding UTF8 $Output
Write-Output "Saved actual process samples to $Output. CPU percent = delta(cpu_seconds_total) / delta(wall_seconds) * 100 per core."
