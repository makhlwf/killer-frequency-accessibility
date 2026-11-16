[CmdletBinding()]
param([string]$BuildDirectory = 'build/tests')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    Get-Command g++ -ErrorAction Stop | Out-Null
    New-Item -ItemType Directory -Force $BuildDirectory | Out-Null
    foreach ($name in @('choice', 'route', 'music_array')) {
        $executable = [IO.Path]::GetFullPath((Join-Path $BuildDirectory ($name + '_test.exe')))
        & g++ -std=c++17 -O2 -static "src/${name}_test.cpp" -o $executable
        if ($LASTEXITCODE) { throw "$name test compilation failed" }
        & $executable
        if ($LASTEXITCODE) { throw "$name tests failed" }
    }
} finally { Pop-Location }
