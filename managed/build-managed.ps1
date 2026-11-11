# ===========================================================================
#  Build EthhhSDK.Managed.dll (the C# mod host) with the VS Roslyn compiler.
#
#  The host targets .NET Framework 4.x (x86), matching the game process.
# ===========================================================================
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'

$root   = Split-Path -Parent $MyInvocation.MyCommand.Path
$src    = Join-Path $root 'src'
$eth    = Split-Path -Parent $root
$dist   = if ($OutputDir) { $OutputDir } else { Join-Path $eth 'build\dist' }

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

# Regenerate the ImGui enums from the vendored header so values never drift.
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if ($python) {
    $maxHeader = Join-Path $eth 'src\third_party\imgui\imgui.h'
    $enumFile  = Join-Path $src 'ImGuiEnums.g.cs'
    & $python (Join-Path $eth 'tools\gen_csharp_enums.py') $maxHeader $enumFile
    if ($LASTEXITCODE -ne 0) { throw 'ImGui enum generation failed.' }
}

if (-not (Test-Path $dist)) { New-Item -ItemType Directory -Path $dist -Force | Out-Null }

$sources = Get-ChildItem -Path $src -Filter *.cs -File | Select-Object -ExpandProperty FullName
if (-not $sources) { throw "No C# sources in $src" }

$out = Join-Path $dist 'EthhhSDK.Managed.dll'
$cscArgs = @(
    '/nologo',
    '/target:library',
    '/platform:x86',
    '/unsafe',
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

Write-Host "Compiling EthhhSDK.Managed.dll ($Configuration)..." -ForegroundColor Cyan
& $csc @cscArgs
if ($LASTEXITCODE -ne 0) { throw "C# build failed ($LASTEXITCODE)." }

Write-Host "Built $out" -ForegroundColor Green
