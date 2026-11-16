$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force bin,build | Out-Null
    & python tools/prepare_map_descriptions.py
    if ($LASTEXITCODE) { throw 'Map description preparation failed' }
    $sources = @('vendor/minhook/src/buffer.c', 'vendor/minhook/src/hook.c', 'vendor/minhook/src/trampoline.c', 'vendor/minhook/src/hde/hde64.c')
    $objects = @()
    foreach ($source in $sources) {
        $object = 'build/' + [IO.Path]::GetFileNameWithoutExtension($source) + '.o'
        & gcc -O2 -c $source -o $object
        if ($LASTEXITCODE) { throw "C compilation failed: $source" }
        $objects += $object
    }
    & g++ -std=c++17 -O2 -Wall -Wextra -Wno-cast-function-type -D_WIN32_WINNT=0x0602 -shared -static src/access.cpp @objects -o bin/KillerFrequencyAccess.dll -luser32
    if ($LASTEXITCODE) { throw 'Accessibility DLL build failed' }
    & g++ -std=c++17 -O2 -static -municode -mwindows src/launcher.cpp -o bin/KillerFrequencyAccessible.exe -luser32
    if ($LASTEXITCODE) { throw 'Launcher build failed' }
    & g++ -std=c++17 -O2 -static src/choice_test.cpp -o build/choice_test.exe
    if ($LASTEXITCODE) { throw 'Choice tests build failed' }
    & ./build/choice_test.exe
    if ($LASTEXITCODE) { throw 'Choice state tests failed' }
    & g++ -std=c++17 -O2 -static src/route_test.cpp -o build/route_test.exe
    if ($LASTEXITCODE) { throw 'Route tests build failed' }
    & ./build/route_test.exe
    if ($LASTEXITCODE) { throw 'Route planning tests failed' }
    Copy-Item -LiteralPath vendor/prism/dynamic/release/bin/prism.dll -Destination bin/prism.dll -Force
    if (-not (Test-Path -LiteralPath bin/nvdaControllerClient64.dll)) {
        Copy-Item -LiteralPath C:/nvgt/lib/nvdaControllerClient64.dll -Destination bin/nvdaControllerClient64.dll
    }
    Write-Output 'Build and choice state tests passed.'
    & ./build_music_fix.ps1
    if ($LASTEXITCODE) { throw 'Music companion build failed' }
} finally { Pop-Location }
