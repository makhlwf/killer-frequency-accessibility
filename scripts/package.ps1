[CmdletBinding()]
param([string]$InputDirectory = 'build/release/bin', [string]$OutputPath = 'dist/KillerFrequencyAccessibility.zip')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $inputPath = [IO.Path]::GetFullPath($InputDirectory)
    $manifest = Get-Content -LiteralPath (Join-Path $inputPath 'build-manifest.json') -Raw | ConvertFrom-Json
    foreach ($name in @('KillerFrequencyAccess.dll', 'KillerFrequencyMusic.dll', 'KillerFrequencyAccessible.exe', 'prism.dll')) {
        if ((Get-FileHash -LiteralPath (Join-Path $inputPath $name) -Algorithm SHA256).Hash -ne $manifest.$name) { throw "Build set changed: $name. Rebuild before packaging." }
    }
    $zip = [IO.Path]::GetFullPath($OutputPath)
    $stage = Join-Path ([IO.Path]::GetTempPath()) ('killer-frequency-package-' + [guid]::NewGuid().ToString('N'))
    $payload = Join-Path $stage 'AccessibilityMod'
    $bin = Join-Path $payload 'bin'
    New-Item -ItemType Directory -Force $bin | Out-Null
    try {
        foreach ($name in @('KillerFrequencyAccess.dll', 'KillerFrequencyMusic.dll', 'KillerFrequencyAccessible.exe', 'prism.dll')) {
            Copy-Item -LiteralPath (Join-Path $inputPath $name) -Destination $bin
        }
        # Explicit allowlist: no game assets, runtime logs, controller DLLs, or historical patches.
        foreach ($name in @('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'ACCESSIBILITY.md', 'CONTRIBUTING.md', 'CHANGELOG.md', 'SECURITY.md', 'CODE_OF_CONDUCT.md', 'dependencies.lock.json', 'supported-build.json')) { Copy-Item -LiteralPath $name -Destination $payload }
        Copy-Item -LiteralPath docs -Destination $payload -Recurse
        New-Item -ItemType Directory -Force "$payload/scripts","$payload/vendor/minhook","$payload/vendor/prism" | Out-Null
        Copy-Item -LiteralPath scripts/install.ps1 -Destination "$payload/scripts"
        Copy-Item -LiteralPath vendor/minhook/LICENSE.txt -Destination "$payload/vendor/minhook"
        Copy-Item -LiteralPath vendor/prism/NOTICE -Destination "$payload/vendor/prism"
        Copy-Item -LiteralPath vendor/prism/LICENSES -Destination "$payload/vendor/prism" -Recurse
        $rows = Get-ChildItem -LiteralPath $payload -File -Recurse | Sort-Object FullName | ForEach-Object {
            (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $_.FullName.Substring($payload.Length + 1).Replace('\','/')
        }
        $rows | Set-Content "$payload/SHA256SUMS.txt" -Encoding ascii
        New-Item -ItemType Directory -Force (Split-Path $zip -Parent) | Out-Null
        Compress-Archive -LiteralPath $payload -DestinationPath $zip -Force
        ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + (Split-Path $zip -Leaf)) | Set-Content "$zip.sha256" -Encoding ascii
        Write-Output "Package: $zip"
    } finally {
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if (-not ([IO.Path]::GetFullPath($stage).StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase))) { throw 'Unsafe staging cleanup path.' }
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
} finally { Pop-Location }
