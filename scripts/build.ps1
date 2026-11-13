param([switch]$Run)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) { $cmakePath = $cmake.Source }
elseif (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmakePath = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
} else { throw 'Install CMake and Visual Studio C++ Build Tools (Desktop development with C++).' }
if (-not (Test-Path $cmakePath)) { throw 'CMake missing. Install the C++ CMake tools component.' }
& $cmakePath -S . -B build -A x64
if ($LASTEXITCODE) { throw 'Configure failed' }
& $cmakePath --build build --config Release
if ($LASTEXITCODE) { throw 'Build failed' }
$ctest = Join-Path (Split-Path $cmakePath) 'ctest.exe'
& $ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed' }
if ($Run) { Start-Process -FilePath (Join-Path $PWD 'build\Release\kindle_sim.exe') -WorkingDirectory $PWD }
