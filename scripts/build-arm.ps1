param([string]$Zig = "$env:LOCALAPPDATA\KindleLite\Toolchains\zig-windows-x86_64-0.13.0\zig.exe")

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
if (-not (Test-Path -LiteralPath $Zig)) { throw 'Install the verified Zig 0.13.0 Windows toolchain first.' }
New-Item -ItemType Directory -Path 'build-arm' -Force | Out-Null
$sources = @('src/library/catalog.cpp', 'src/reader/text_reader.cpp', 'src/core/state.cpp', 'src/core/device_status.cpp', 'src/eink/refresh.cpp')
$flags = @('-target', 'arm-linux-musleabihf', '-mcpu=cortex_a9', '-static', '-Os', '-std=c++17', '-Iinclude')
$documentFlags = @('-Ithird_party/tinyxml2', '-Ithird_party/miniz', '-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES')
$documentSources = @('src/reader/document.cpp', 'third_party/tinyxml2/tinyxml2.cpp')
foreach ($name in @('miniz','miniz_tdef','miniz_tinfl','miniz_zip')) {
    $object = "build-arm/$name.o"
    & $Zig cc -target arm-linux-musleabihf -mcpu=cortex_a9 -Os @documentFlags -c "third_party/miniz/$name.c" -o $object
    if ($LASTEXITCODE) { throw "ARM dependency build failed: $name" }
    $documentSources += $object
}
& $Zig c++ @flags @sources src/core/main.cpp -o build-arm/kindle_cli
if ($LASTEXITCODE) { throw 'ARM CLI build failed' }
& $Zig c++ @flags @sources tests/core_tests.cpp -o build-arm/core_tests
if ($LASTEXITCODE) { throw 'ARM test build failed' }
& $Zig c++ @flags @sources tests/status_tests.cpp -o build-arm/status_tests
if ($LASTEXITCODE) { throw 'ARM status test build failed' }
& $Zig c++ @flags @documentFlags @sources @documentSources tests/document_tests.cpp -o build-arm/document_tests
if ($LASTEXITCODE) { throw 'ARM document test build failed' }
& $Zig c++ @flags src/browser/web.cpp tests/web_tests.cpp -o build-arm/web_tests
if ($LASTEXITCODE) { throw 'ARM web test build failed' }
& $Zig c++ @flags @documentFlags @sources @documentSources src/browser/web.cpp src/browser/linux.cpp src/reader/native.cpp src/ui/kindle.cpp -o build-arm/kindle_ui
if ($LASTEXITCODE) { throw 'ARM device UI build failed' }
