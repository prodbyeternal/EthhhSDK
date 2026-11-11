# ===========================================================================
#  Install EthhhSDK into the I-Ninja game folder
#
#  Copies : build\dist\dinput8.dll   -> <game>\dinput8.dll
#           build\dist\EthhhSDK.dll  -> <game>\EthhhSDK.dll
#           build\dist\EthhhSDK.Managed.dll -> <game>\EthhhSDK\EthhhSDK.Managed.dll
#           build\dist\mods\<name>\<entry> -> <game>\EthhhSDK\mods\<name>\<entry>
#  Backs up any file it overwrites into <game>\EthhhSDK\_backup\<timestamp>.
# ===========================================================================
[CmdletBinding()]
param(
    [string]$GameDir = 'C:\Users\Administrator\Desktop\ininja',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dist = Join-Path $root 'build\dist'

$required = @('dinput8.dll', 'EthhhSDK.dll')
foreach ($f in $required) {
    if (-not (Test-Path (Join-Path $dist $f))) {
        throw "Missing $f in $dist - run .\build.ps1 first."
    }
}

if (-not (Test-Path $GameDir)) {
    throw "Game directory not found: $GameDir"
}
if (-not (Test-Path (Join-Path $GameDir 'I-Ninja.exe'))) {
    throw "I-Ninja.exe not found in $GameDir"
}

$running = Get-Process -Name 'I-Ninja' -ErrorAction SilentlyContinue
if ($running) {
    if (-not $Force) {
        throw "I-Ninja.exe is running. Close the game or pass -Force to terminate it."
    }
    Write-Host "Stopping I-Ninja.exe..." -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 400
}

$stamp  = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path $GameDir "EthhhSDK\_backup\$stamp"

$copies = @('dinput8.dll', 'EthhhSDK.dll')
if (Test-Path (Join-Path $dist 'EthhhSDK.Managed.dll')) {
    $copies += 'EthhhSDK.Managed.dll'
}

foreach ($name in $copies) {
    $src = Join-Path $dist $name
    $dst = if ($name -eq 'EthhhSDK.Managed.dll') { Join-Path $GameDir "EthhhSDK\$name" } else { Join-Path $GameDir $name }
    if (-not (Test-Path (Split-Path -Parent $dst))) { New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null }
    if (Test-Path $dst) {
        if (-not (Test-Path $backup)) { New-Item -ItemType Directory -Path $backup -Force | Out-Null }
        Copy-Item $dst (Join-Path $backup $name) -Force
    }
    Copy-Item $src $dst -Force
    Write-Host "installed $name" -ForegroundColor Green
}

# Deploy only staged assembly outputs; user configuration and mod sources stay
# untouched. Keep mod backups under their relative path (not all as Mod.dll).
$stagedMods = Join-Path $dist 'mods'
if (Test-Path $stagedMods) {
    foreach ($mod in Get-ChildItem $stagedMods -Directory) {
        $manifestPath = Join-Path $mod.FullName 'mod.json'
        if (-not (Test-Path $manifestPath)) { continue }
        $manifest = Get-Content -Raw $manifestPath | ConvertFrom-Json
        $entry = if ($manifest.entry) { $manifest.entry } else { 'Mod.dll' }
        $src = [IO.Path]::GetFullPath((Join-Path $mod.FullName $entry))
        $srcRoot = [IO.Path]::GetFullPath($mod.FullName).TrimEnd('\') + '\'
        if (-not $src.StartsWith($srcRoot, [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path $src -PathType Leaf)) {
            throw "Missing or invalid staged mod entry: $entry"
        }
        $relative = "mods\$($mod.Name)\$entry"
        $dst = Join-Path $GameDir "EthhhSDK\$relative"
        New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
        if (Test-Path $dst) {
            $saved = Join-Path $backup $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
            Copy-Item $dst $saved -Force
        }
        Copy-Item $src $dst -Force
        # Do not overwrite a live, potentially user-edited manifest.
        $liveManifest = Join-Path (Join-Path $GameDir "EthhhSDK\mods\$($mod.Name)") 'mod.json'
        if (-not (Test-Path $liveManifest)) { Copy-Item $manifestPath $liveManifest }
        Write-Host "installed $relative" -ForegroundColor Green
    }
}

foreach ($dir in @('EthhhSDK', 'EthhhSDK\mods', 'EthhhSDK\config', 'EthhhSDK\logs')) {
    $path = Join-Path $GameDir $dir
    if (-not (Test-Path $path)) { New-Item -ItemType Directory -Path $path -Force | Out-Null }
}

if (Test-Path $backup) {
    Write-Host "backup: $backup" -ForegroundColor DarkGray
}
Write-Host "Done. Launch $GameDir\I-Ninja.exe" -ForegroundColor Cyan
