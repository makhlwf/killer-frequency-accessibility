[CmdletBinding()]
param([string]$OutputDirectory = 'bin', [string]$NvdaControllerPath)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    foreach ($tool in @('gcc', 'g++', 'python', 'nm')) { Get-Command $tool -ErrorAction Stop | Out-Null }
    $target = (& gcc -dumpmachine).Trim()
    if ($LASTEXITCODE -or $target -notmatch '^x86_64.*mingw') { throw "A Windows x64 MinGW GCC toolchain is required; found $target" }
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    $work = Join-Path (Split-Path $output -Parent) ((Split-Path $output -Leaf) + '-objects')
    New-Item -ItemType Directory -Force $output,$work | Out-Null
    & ./scripts/setup-dependencies.ps1
    & python tools/prepare_map_descriptions.py
    if ($LASTEXITCODE) { throw 'Map description preparation failed' }
    $sources = @('buffer', 'hook', 'trampoline', 'hde/hde64')
    $objects = @()
    foreach ($source in $sources) {
        $object = Join-Path $work ((Split-Path $source -Leaf) + '.o')
        & gcc -O2 -c "vendor/minhook/src/$source.c" -o $object
        if ($LASTEXITCODE) { throw "C compilation failed: $source" }
        $objects += $object
    }
    & g++ -std=c++17 -O2 -Wall -Wextra -Wno-cast-function-type -D_WIN32_WINNT=0x0602 -shared -static src/access.cpp @objects -o "$output/KillerFrequencyAccess.dll" -luser32
    if ($LASTEXITCODE) { throw 'Accessibility DLL build failed (close the game before replacing loaded DLLs).' }
    & ./build_music_fix.ps1 -OutputDirectory $output -BuildDirectory $work
    & g++ -std=c++17 -O2 -static -municode -mwindows src/launcher.cpp -o "$output/KillerFrequencyAccessible.exe" -luser32
    if ($LASTEXITCODE) { throw 'Launcher build failed' }
    Copy-Item -LiteralPath vendor/prism/dynamic/release/bin/prism.dll -Destination "$output/prism.dll" -Force
    if ($NvdaControllerPath) { Copy-Item -LiteralPath $NvdaControllerPath -Destination "$output/nvdaControllerClient64.dll" -Force }
    & ./scripts/test.ps1 -BuildDirectory (Join-Path $work 'tests')
    $manifest = [ordered]@{}
    foreach ($name in @('KillerFrequencyAccess.dll', 'KillerFrequencyMusic.dll', 'KillerFrequencyAccessible.exe', 'prism.dll')) {
        $manifest[$name] = (Get-FileHash -LiteralPath (Join-Path $output $name) -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $manifest | ConvertTo-Json | Set-Content (Join-Path $output 'build-manifest.json') -Encoding ascii
    Write-Output "Build and all native tests passed: $output"
} finally { Pop-Location }
