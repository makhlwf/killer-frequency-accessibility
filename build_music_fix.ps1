$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & python tools/music_symbols.py
    if ($LASTEXITCODE) { throw 'Music patch manifest preparation failed' }
    & g++ -std=c++17 -O2 -static src/music_array_test.cpp -o build/music_array_test.exe
    if ($LASTEXITCODE) { throw 'Music metadata array test build failed' }
    & ./build/music_array_test.exe
    if ($LASTEXITCODE) { throw 'Music metadata array test failed' }
    $objects = @('build/buffer.o','build/hook.o','build/trampoline.o','build/hde64.o')
    & g++ -std=c++17 -O2 -Wall -Wextra -Wno-cast-function-type -D_WIN32_WINNT=0x0602 -shared -static src/music_fix.cpp @objects -o bin/KillerFrequencyMusicFix-v22.dll -luser32
    if ($LASTEXITCODE) { throw 'Music patch build failed' }
    & g++ -std=c++17 -O2 -static -municode -mwindows src/launcher.cpp -o bin/KillerFrequencyAccessible.exe -luser32
    if ($LASTEXITCODE) { throw 'Launcher build failed' }
    Write-Output 'Live music patch and updated launcher built.'
} finally { Pop-Location }
