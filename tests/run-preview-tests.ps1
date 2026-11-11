[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [string]$OutputDir = "$env:LOCALAPPDATA\Temp\opencode"
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$build = Join-Path $root 'build'
if (-not (Test-Path "$build\CMakeCache.txt")) { throw 'Build the SDK and enable tests first.' }
$cmake = (Get-Command cmake).Source
& $cmake --build $build --config $Configuration --target eth_chrome_preview --parallel
if ($LASTEXITCODE -ne 0) { throw 'Chrome preview build failed.' }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$preview = Join-Path $build 'tests\preview\eth_chrome_preview.exe'
$cases = @(
    @('standard',1152,704,1,0,0),
    @('narrow',900,640,1,0,0),
    @('hidpi',1728,1056,1.5,0,0),
    @('stress',1152,704,1,1,0),
    @('narrow-stress',900,640,1,1,0),
    @('patch',1152,704,1,0,1),
    @('patch-narrow',900,640,1,0,1),
    @('patch-hidpi',1728,1056,1.5,0,1),
    @('patch-fixture',1152,704,1,1,1),
    @('patch-fixture-narrow',900,640,1,1,1),
    @('patch-fixture-hidpi',1728,1056,1.5,1,1)
)
foreach ($case in $cases) {
    & $preview (Join-Path $OutputDir "ethhh-chrome-$($case[0]).png") $case[1] $case[2] $case[3] $case[4] $case[5]
    if ($LASTEXITCODE -ne 0) { throw "Chrome/routing regression failed: $($case[0])" }
}
