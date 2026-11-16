[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dependency = (Get-Content (Join-Path $root 'dependencies.lock.json') -Raw | ConvertFrom-Json).prism
$archive = Join-Path $root 'vendor/prism-windows-x64.zip'
if (-not (Test-Path -LiteralPath $archive)) {
    New-Item -ItemType Directory -Force (Split-Path $archive -Parent) | Out-Null
    try { Invoke-WebRequest -Uri $dependency.url -OutFile $archive -TimeoutSec 180 }
    catch {
        if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
        throw
    }
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $dependency.sha256) {
    throw "Prism archive checksum mismatch: $archive. Remove it and retry."
}
$destination = Join-Path $root 'vendor/prism'
# Re-extraction also repairs missing files; never trusts an unverified cached DLL.
Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
if (-not (Test-Path -LiteralPath (Join-Path $destination 'dynamic/release/bin/prism.dll'))) {
    throw 'The verified Prism archive has an unexpected layout.'
}
Write-Output "Verified Prism $($dependency.version)."
