param([string]$Version='0.1-alpha', [string]$ZigRoot="$env:LOCALAPPDATA\KindleLite\Toolchains\zig-windows-x86_64-0.13.0")
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
if($Version -notmatch '^[0-9A-Za-z.-]+$') { throw 'Invalid package version' }
$output=Join-Path $PWD 'dist'
$stage=Join-Path $output ('staging/'+$Version+'-'+[DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff'))
$device=Join-Path $stage 'pw4/kindle-lite'
$windows=Join-Path $stage 'windows'
New-Item -ItemType Directory -Force -Path $device,$windows,(Join-Path $device 'books'),(Join-Path $windows 'books'),(Join-Path $device 'licenses'),(Join-Path $windows 'licenses') | Out-Null
foreach($file in @('kindle_ui','kindle_cli')) { Copy-Item -LiteralPath "build-arm/$file" -Destination $device }
foreach($pair in @(@('kindle-launch.sh','launch.sh'),@('kindle-boot.sh','boot.sh'),@('kindle-power-bridge.lua','power-bridge.lua'),@('kindle-install-autostart.sh','install-autostart.sh'))) {
    Copy-Item -LiteralPath ('scripts/'+$pair[0]) -Destination (Join-Path $device $pair[1])
}
Copy-Item -LiteralPath config/kindle-lite.conf -Destination $device
Copy-Item -LiteralPath assets/Welcome-Kindle.txt -Destination (Join-Path $device 'books/Welcome.txt')
Copy-Item -LiteralPath books/Welcome.txt -Destination (Join-Path $windows 'books/Welcome.txt')
Copy-Item -LiteralPath build/Release/kindle_sim.exe,build/Release/kindle_cli.exe -Destination $windows
foreach($root in @($device,$windows)) {
    Copy-Item -LiteralPath assets/sample-books/Sample.epub,assets/sample-books/Sample.fb2 -Destination (Join-Path $root 'books')
    Copy-Item -LiteralPath docs/releases/0.1-alpha.md -Destination (Join-Path $root 'README.md')
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
[IO.File]::WriteAllLines((Join-Path $output 'SHA256SUMS.txt'),$checksums,(New-Object Text.UTF8Encoding($false)))
Get-Item -LiteralPath $deviceArchive,$windowsArchive,(Join-Path $output 'SHA256SUMS.txt') | Select-Object Name,Length
