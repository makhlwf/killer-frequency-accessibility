[CmdletBinding()]
param([string]$OutputDirectory = 'bin', [string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    foreach ($tool in @('g++', 'python', 'nm')) { Get-Command $tool -ErrorAction Stop | Out-Null }
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    if (-not $BuildDirectory) { $BuildDirectory = Join-Path (Split-Path $output -Parent) ((Split-Path $output -Leaf) + '-objects') }
    $work = [IO.Path]::GetFullPath($BuildDirectory)
    New-Item -ItemType Directory -Force $work | Out-Null
    $objects = @('buffer.o', 'hook.o', 'trampoline.o', 'hde64.o') | ForEach-Object { Join-Path $work $_ }
    foreach ($object in $objects) { if (-not (Test-Path -LiteralPath $object)) { throw 'Run build.ps1 first to compile MinHook.' } }
    if ((Get-Content src/music_fix.cpp -Raw) -notmatch '#include\s*<music_symbols\.h>') { throw 'music_fix.cpp must include <music_symbols.h> so the matching generated build manifest is used.' }
    & python tools/music_symbols.py --dll "$output/KillerFrequencyAccess.dll" --output "$work/music_symbols.h"
    if ($LASTEXITCODE) { throw 'Music patch manifest preparation failed' }
    & g++ -std=c++17 -O2 -Wall -Wextra -Wno-cast-function-type -D_WIN32_WINNT=0x0602 -shared -static "-I$work" src/music_fix.cpp @objects -o "$output/KillerFrequencyMusic.dll" -luser32
    if ($LASTEXITCODE) { throw 'Music companion build failed' }
    Write-Output 'Music companion bound to the matching accessibility DLL.'
} finally { Pop-Location }
