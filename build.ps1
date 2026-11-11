# ===========================================================================
#  Build EthhhSDK (Win32 / MSVC)
# ===========================================================================
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Reconfigure,
    [switch]$VerboseOutput,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$root  = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root 'build'
$dist  = Join-Path $build 'dist'

if ($Clean -and (Test-Path $build)) {
    Write-Host "Removing $build" -ForegroundColor Yellow
    Remove-Item $build -Recurse -Force
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio 2022 with the C++ (x86/x64) workload."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "No Visual Studio installation with the C++ x86 toolset was found."
}

Write-Host "Visual Studio: $vsPath" -ForegroundColor DarkGray

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { throw "cmake.exe was not found on PATH." }

$cache = Join-Path $build 'CMakeCache.txt'
if ($Reconfigure -or -not (Test-Path $cache)) {
    Write-Host "Configuring (Visual Studio 17 2022, Win32)..." -ForegroundColor Cyan
    & $cmake -S $root -B $build -G "Visual Studio 17 2022" -A Win32
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)." }
} else {
    Write-Host "Using existing CMake cache (use -Reconfigure or -Clean to regenerate)." -ForegroundColor DarkGray
}

Write-Host "Building $Configuration..." -ForegroundColor Cyan
& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }

# C# mod host (IL, .NET Framework 4.x). Built with Roslyn; optional but enabled
# by default so C# mods work out of the box.
$managed = Join-Path $root 'managed\build-managed.ps1'
if (Test-Path $managed) {
    Write-Host ""
    & powershell -NoProfile -ExecutionPolicy Bypass -File $managed -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Managed host build failed ($LASTEXITCODE)." }
}

# C# mods (compiled against build\dist\EthhhSDK.Managed.dll).
$modsBuild = Join-Path $root 'mods\build-mods.ps1'
if (Test-Path $modsBuild) {
    Write-Host ""
    & powershell -NoProfile -ExecutionPolicy Bypass -File $modsBuild -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Mod build failed ($LASTEXITCODE)." }
}

if (-not (Test-Path $dist)) { throw "Build finished but $dist does not exist." }

Write-Host ""
Write-Host "Artifacts in ${dist}:" -ForegroundColor Green
Get-ChildItem $dist -File | Sort-Object Name | Format-Table Name, Length, LastWriteTime -AutoSize
