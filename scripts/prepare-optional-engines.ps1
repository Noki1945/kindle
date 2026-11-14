param(
    [string]$Destination = (Join-Path $env:LOCALAPPDATA 'KindleLite\Prepared'),
    [string]$Cache = (Join-Path $env:LOCALAPPDATA 'KindleLite\EngineDownloads')
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$packageRoot = Split-Path $PSScriptRoot -Parent
$manifest = Get-Content -LiteralPath (Join-Path $packageRoot 'config/optional-engines.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$cacheRoot = [IO.Path]::GetFullPath($Cache)
$prepared = Join-Path ([IO.Path]::GetFullPath($Destination)) ('kindle-engines-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $cacheRoot,$prepared | Out-Null

function Get-VerifiedFile([string]$Url, [string]$Sha256, [string]$Suffix) {
    if ($Sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'Missing engine SHA-256' }
    $file = Join-Path $cacheRoot ($Sha256.ToLowerInvariant() + $Suffix)
    if (Test-Path -LiteralPath $file) {
        if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -eq $Sha256) { return $file }
        throw "Cached engine checksum differs: $file"
    }
    $partial = $file + '.part'
    Invoke-WebRequest -Uri $Url -OutFile $partial -UseBasicParsing
    if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $Sha256) { throw "Engine checksum differs: $Url" }
    Move-Item -LiteralPath $partial -Destination $file
    return $file
}

$browserArchive = Get-VerifiedFile $manifest.browser.url $manifest.browser.sha256 '.zip'
$readerArchive = Get-VerifiedFile $manifest.reader.url $manifest.reader.sha256 '.zip'
$browser = Join-Path $prepared 'kindle-lite/engines/browser'
$reader = Join-Path $prepared 'kindle-lite/engines'
$image = Join-Path $prepared 'kbrowser'
New-Item -ItemType Directory -Force -Path $browser,$reader,$image | Out-Null
Expand-Archive -LiteralPath $browserArchive -DestinationPath $image
$downloadedImage = Join-Path $image 'kbrowser-chromium.ext3'
if (Test-Path -LiteralPath $downloadedImage) { Move-Item -LiteralPath $downloadedImage -Destination (Join-Path $image 'kbrowser.ext3') }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($readerArchive)
try {
    $readerRoot = [IO.Path]::GetFullPath($reader).TrimEnd('\') + '\'
    foreach ($entry in $archive.Entries) {
        if (-not $entry.FullName.StartsWith('koreader/') -or -not $entry.Name) { continue }
        $target = [IO.Path]::GetFullPath((Join-Path $reader $entry.FullName))
        if (-not $target.StartsWith($readerRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Reader archive path escaped destination' }
        New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target)
    }
} finally { $archive.Dispose() }
if (-not (Test-Path -LiteralPath (Join-Path $image 'kbrowser.ext3'))) { throw 'Browser image is missing from archive' }
if (-not (Test-Path -LiteralPath (Join-Path $reader 'koreader/luajit'))) { throw 'Reader executable is missing from archive' }
foreach ($component in $manifest.browser.files) {
    if ($component.name -notmatch '^[a-zA-Z0-9_.-]+$') { throw 'Invalid engine component filename' }
    $file = Get-VerifiedFile $component.url $component.sha256 '.txt'
    Copy-Item -LiteralPath $file -Destination (Join-Path $browser $component.name)
}
Copy-Item -LiteralPath (Join-Path $packageRoot 'config/full-browser.conf') -Destination (Join-Path $image 'kbrowser.conf')
Write-Output "Prepared engine files: $prepared"
Write-Output 'Copy its kindle-lite and kbrowser folders to the Kindle user storage, then follow the release installation instructions.'
