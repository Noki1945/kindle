param([string]$Version='0.2-alpha', [string]$ZigRoot="$env:LOCALAPPDATA\KindleLite\Toolchains\zig-windows-x86_64-0.13.0")
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
if($Version -notmatch '^[0-9A-Za-z.-]+$') { throw 'Invalid package version' }
$output=Join-Path $PWD 'dist'
$stage=Join-Path $output ('staging/'+$Version+'-'+[DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff'))
$device=Join-Path $stage 'pw4/kindle-lite'
$windows=Join-Path $stage 'windows'
$releaseNotes=Join-Path $PWD ('docs/releases/'+$Version+'.md')
if(-not(Test-Path -LiteralPath $releaseNotes)) { throw 'Release notes missing for requested version' }
New-Item -ItemType Directory -Force -Path $device,$windows,(Join-Path $device 'books'),(Join-Path $windows 'books'),(Join-Path $device 'licenses'),(Join-Path $windows 'licenses') | Out-Null
foreach($file in @('kindle_ui','kindle_cli')) { Copy-Item -LiteralPath "build-arm/$file" -Destination $device }
foreach($pair in @(@('kindle-launch.sh','launch.sh'),@('kindle-boot.sh','boot.sh'),@('kindle-power-bridge.lua','power-bridge.lua'),@('kindle-install-autostart.sh','install-autostart.sh'),@('kindle-ensure-usb-ssh.sh','ensure-usb-ssh.sh'),@('kindle-disable-direct-boot.sh','disable-direct-boot.sh'),@('kindle-wake.sh','wake.sh'),@('kindle-reader-worker.lua','reader-worker.lua'),@('kindle-full-browser.sh','full-browser.sh'),@('kindle-browser-memory.js','browser-memory.js'),@('kindle-browser-page.js','browser-page.js'),@('kindle-configure-browser.py','configure-browser.py'))) {
    Copy-Item -LiteralPath ('scripts/'+$pair[0]) -Destination (Join-Path $device $pair[1])
}
Copy-Item -LiteralPath config/kindle-lite.conf,config/kindle-lite-wake.conf,config/chromium-pw4.conf -Destination $device
New-Item -ItemType Directory -Force -Path (Join-Path $device 'scripts'),(Join-Path $device 'config') | Out-Null
Copy-Item -LiteralPath scripts/prepare-optional-engines.ps1 -Destination (Join-Path $device 'scripts')
Copy-Item -LiteralPath config/optional-engines.json,config/full-browser.conf -Destination (Join-Path $device 'config')
Copy-Item -LiteralPath third_party/GPL-3.0.txt -Destination (Join-Path $device 'licenses/GPL-3.0.txt')
Copy-Item -LiteralPath assets/Welcome-Kindle.txt -Destination (Join-Path $device 'books/Welcome.txt')
Copy-Item -LiteralPath books/Welcome.txt -Destination (Join-Path $windows 'books/Welcome.txt')
Copy-Item -LiteralPath build/Release/kindle_sim.exe,build/Release/kindle_cli.exe -Destination $windows
foreach($root in @($device,$windows)) {
    Copy-Item -LiteralPath assets/sample-books/Sample.epub,assets/sample-books/Sample.fb2 -Destination (Join-Path $root 'books')
    Copy-Item -LiteralPath $releaseNotes -Destination (Join-Path $root 'README.md')
    Copy-Item -LiteralPath third_party/tinyxml2/LICENSE.txt -Destination (Join-Path $root 'licenses/tinyxml2.txt')
    Copy-Item -LiteralPath third_party/miniz/LICENSE -Destination (Join-Path $root 'licenses/miniz.txt')
}
foreach($pair in @(@('lib/libc/musl/COPYRIGHT','musl.txt'),@('lib/libcxx/LICENSE.TXT','libcxx.txt'),@('lib/libcxxabi/LICENSE.TXT','libcxxabi.txt'),@('lib/libunwind/LICENSE.TXT','libunwind.txt'),@('LICENSE','zig.txt'))) {
    Copy-Item -LiteralPath (Join-Path $ZigRoot $pair[0]) -Destination (Join-Path $device ('licenses/'+$pair[1]))
}
$deviceArchive=Join-Path $output "kindle-lite-$Version-pw4.zip"
$windowsArchive=Join-Path $output "kindle-lite-$Version-windows-x64.zip"
foreach($archive in @($deviceArchive,$windowsArchive)) { if(Test-Path -LiteralPath $archive) { throw "Archive already exists: $archive" } }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Write-PackageZip([string]$Source, [string]$Destination) {
    $sourceRoot=[IO.Path]::GetFullPath($Source).TrimEnd('\','/')
    $zip=[IO.Compression.ZipFile]::Open($Destination,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
            $relative=$file.FullName.Substring($sourceRoot.Length+1).Replace('\','/')
            $entry=$zip.CreateEntry($relative,[IO.Compression.CompressionLevel]::Optimal)
            $input=[IO.File]::OpenRead($file.FullName); $target=$entry.Open()
            try { $input.CopyTo($target) } finally { $target.Dispose(); $input.Dispose() }
        }
    } finally { $zip.Dispose() }
}
Write-PackageZip (Split-Path $device) $deviceArchive
Write-PackageZip $windows $windowsArchive
$checksums=foreach($archive in @($deviceArchive,$windowsArchive)) { (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($archive) }
$checksumFile=Join-Path $output "SHA256SUMS-$Version.txt"
if(Test-Path -LiteralPath $checksumFile) { throw "Checksum file already exists: $checksumFile" }
[IO.File]::WriteAllLines($checksumFile,$checksums,(New-Object Text.UTF8Encoding($false)))
Get-Item -LiteralPath $deviceArchive,$windowsArchive,$checksumFile | Select-Object Name,Length
