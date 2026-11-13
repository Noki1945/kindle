param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemoteCommand
)

$ErrorActionPreference = 'Stop'
$connectionDirectory = Join-Path $env:LOCALAPPDATA 'KindleLite\USBNetLite'
$privateKey = Join-Path $connectionDirectory 'kindle_ed25519'
$knownHosts = Join-Path $connectionDirectory 'known_hosts'
if (-not (Test-Path -LiteralPath $privateKey)) {
    throw 'Kindle SSH key is missing. Complete the device connection setup first.'
}

$sshArguments = @(
    '-i', $privateKey,
    '-o', 'IdentitiesOnly=yes',
    '-o', 'StrictHostKeyChecking=yes',
    '-o', "UserKnownHostsFile=$knownHosts",
    '-o', 'ConnectTimeout=8',
    'root@192.168.15.244'
)
& ssh.exe @sshArguments @RemoteCommand
exit $LASTEXITCODE
