[CmdletBinding()]
param([Parameter(Mandatory)][string]$GameDirectory)
$ErrorActionPreference = 'Stop'
$source = Split-Path $PSScriptRoot -Parent
$game = [IO.Path]::GetFullPath($GameDirectory)
if (-not (Test-Path -LiteralPath (Join-Path $game 'KillerFrequency.exe'))) { throw 'Choose the folder containing KillerFrequency.exe.' }
$gameExecutable = Join-Path $game 'KillerFrequency.exe'
$running = Get-Process -Name KillerFrequency -ErrorAction SilentlyContinue | Where-Object {
    -not $_.Path -or [string]::Equals($_.Path, $gameExecutable, [StringComparison]::OrdinalIgnoreCase)
}
if ($running) { throw 'Close Killer Frequency before installing into this game folder.' }
$manifest = Join-Path $source 'SHA256SUMS.txt'
if (-not (Test-Path -LiteralPath $manifest)) { throw 'Run this installer from an extracted release package.' }
foreach ($line in Get-Content -LiteralPath $manifest) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') { throw 'Invalid checksum manifest.' }
    $expected = $Matches[1]
    $relative = $Matches[2]
    $path = [IO.Path]::GetFullPath((Join-Path $source $relative))
    if (-not $path.StartsWith($source + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe manifest path.' }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected) { throw "Checksum mismatch: $relative" }
}
$destination = Join-Path $game 'AccessibilityMod'
if ([IO.Path]::GetFullPath($source) -eq [IO.Path]::GetFullPath($destination)) { Write-Output 'Package already installed and verified.'; return }
New-Item -ItemType Directory -Force $destination | Out-Null
foreach ($name in @('bin', 'docs', 'vendor', 'scripts', 'README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'ACCESSIBILITY.md', 'CONTRIBUTING.md', 'CHANGELOG.md', 'SECURITY.md', 'CODE_OF_CONDUCT.md', 'dependencies.lock.json', 'supported-build.json', 'SHA256SUMS.txt')) {
    Copy-Item -LiteralPath (Join-Path $source $name) -Destination $destination -Recurse -Force
}
Write-Output "Installed. Launch $destination/bin/KillerFrequencyAccessible.exe"
