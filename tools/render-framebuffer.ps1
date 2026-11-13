param([Parameter(Mandatory=$true)][string]$InputFile, [Parameter(Mandatory=$true)][string]$OutputFile)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$data = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $InputFile))
$offset = 0
$lines = New-Object 'Collections.Generic.List[string]'
for ($row=0; $row -lt 3; $row++) {
    $begin=$offset
    while ($offset -lt $data.Length -and $data[$offset] -ne 10) { $offset++ }
    if ($offset -ge $data.Length) { throw 'Invalid PGM header' }
    $lines.Add([Text.Encoding]::ASCII.GetString($data, $begin, $offset-$begin)); $offset++
}
if ($lines[0] -ne 'P5' -or $lines[2] -ne '255') { throw 'Only 8-bit P5 PGM is supported' }
$dimensions=$lines[1].Split(' ')
$width=[int]$dimensions[0]; $height=[int]$dimensions[1]
if ($width -le 0 -or $height -le 0 -or $width -gt 4096 -or $height -gt 4096 -or $data.Length-$offset -ne $width*$height) { throw 'Invalid framebuffer dimensions' }
$bitmap=New-Object Drawing.Bitmap($width,$height,[Drawing.Imaging.PixelFormat]::Format8bppIndexed)
try {
    $palette=$bitmap.Palette
    for ($i=0; $i -lt 256; $i++) { $palette.Entries[$i]=[Drawing.Color]::FromArgb($i,$i,$i) }
    $bitmap.Palette=$palette
    $rectangle=New-Object Drawing.Rectangle(0,0,$width,$height)
    $bits=$bitmap.LockBits($rectangle,[Drawing.Imaging.ImageLockMode]::WriteOnly,[Drawing.Imaging.PixelFormat]::Format8bppIndexed)
    try {
        for ($y=0; $y -lt $height; $y++) { [Runtime.InteropServices.Marshal]::Copy($data,$offset+$y*$width,[IntPtr]::Add($bits.Scan0,$y*$bits.Stride),$width) }
    } finally { $bitmap.UnlockBits($bits) }
    $bitmap.Save([IO.Path]::GetFullPath($OutputFile),[Drawing.Imaging.ImageFormat]::Png)
} finally { $bitmap.Dispose() }
