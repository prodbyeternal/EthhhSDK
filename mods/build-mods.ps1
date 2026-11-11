# ===========================================================================
#  Stage C# mods under build\dist\mods\<name>\<entry>. Never write live DLLs.
#
#  Mods compile against build\dist\EthhhSDK.Managed.dll (the SDK API). The
#  host assembly is NOT copied into the mod folder: the CLR already has it
#  loaded under the same identity, which is what makes IEthMod binding work.
#
#  Usage:  .\build-mods.ps1 [-Configuration Release|Debug]
# ===========================================================================
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$eth  = Split-Path -Parent $root
$dist = Join-Path $eth 'build\dist'
$sdkApi = Join-Path $dist 'EthhhSDK.Managed.dll'

if (-not (Test-Path $sdkApi)) {
    throw "Missing $sdkApi - run .\build.ps1 or .\managed\build-managed.ps1 first."
}

# --- locate the compiler ----------------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}

$cscCandidates = @()
if ($vsPath) { $cscCandidates += (Join-Path $vsPath 'MSBuild\Current\Bin\Roslyn\csc.exe') }
$cscCandidates += (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe')
$csc = $cscCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $csc) { throw 'No C# compiler found (Roslyn or .NET Framework csc.exe).' }

$framework = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319'
$refs = @('mscorlib.dll', 'System.dll', 'System.Core.dll') | ForEach-Object {
    $path = Join-Path $framework $_
    if (-not (Test-Path $path)) { throw "Missing reference assembly: $path" }
    "/reference:`"$path`""
}
$refs += "/reference:`"$sdkApi`""

# --- discover mods ----------------------------------------------------------
$manifests = Get-ChildItem -Path $root -Directory |
    ForEach-Object { Join-Path $_.FullName 'mod.json' } |
    Where-Object { Test-Path $_ }

$built = 0
foreach ($manifestPath in $manifests) {
    $modDir = Split-Path -Parent $manifestPath
    $manifest = Get-Content -Raw $manifestPath | ConvertFrom-Json

    if ($manifest.type -ne 'csharp') { continue }

    $srcDir = Join-Path $modDir 'src'
    if (-not (Test-Path $srcDir)) { continue }

    $sources = Get-ChildItem -Path $srcDir -Filter *.cs -File | Select-Object -ExpandProperty FullName
    if (-not $sources) { continue }

    $entry = if ($manifest.entry) { $manifest.entry } else { 'Mod.dll' }
    $stageDir = Join-Path $dist ('mods\' + (Split-Path -Leaf $modDir))
    $out = [IO.Path]::GetFullPath((Join-Path $stageDir $entry))
    $stageRoot = [IO.Path]::GetFullPath($stageDir).TrimEnd('\') + '\'
    if (-not $out.StartsWith($stageRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Mod entry must stay inside its staged folder: $entry"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $out) -Force | Out-Null

    $cscArgs = @(
        '/nologo',
        '/target:library',
        '/platform:x86',
        '/langversion:latest'
    )
    if ($Configuration -eq 'Debug') {
        $cscArgs += @('/debug+', '/optimize-')
    } else {
        $cscArgs += '/optimize+'
    }
    $cscArgs += $refs
    $cscArgs += "/out:`"$out`""
    $cscArgs += $sources

    Write-Host "Compiling mod '$($manifest.name)' -> $entry ..." -ForegroundColor Cyan
    & $csc @cscArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Mod build failed for '$($manifest.name)' ($LASTEXITCODE)."
    }
    Copy-Item $manifestPath (Join-Path $stageDir 'mod.json') -Force
    $license = Join-Path $modDir 'LICENSE'
    if (Test-Path $license -PathType Leaf) { Copy-Item $license (Join-Path $stageDir 'LICENSE') -Force }
    $built++
}

if ($built -eq 0) {
    Write-Host "No C# mods with sources found." -ForegroundColor DarkGray
} else {
    Write-Host "Built $built C# mod(s)." -ForegroundColor Green
}
