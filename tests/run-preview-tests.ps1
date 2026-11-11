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
    @('patch-fixture-hidpi',1728,1056,1.5,1,1),
    @('camera',1152,704,1,0,1,1),
    @('camera-narrow',900,640,1,0,1,1),
    @('camera-hidpi',1728,1056,1.5,0,1,1),
    @('camera-small',640,480,1,0,1,1),
    @('camera-bottom',900,640,1,0,1,1,1),
    @('camera-1080p',1920,1080,1,0,1,1),
    @('camera-4k',3840,2160,1,0,1,1),
    @('camera-4k-hidpi',3840,2160,2,0,1,1),
    @('animation',1152,704,1,0,1,2),
    @('animation-narrow',900,640,1,0,1,2),
    @('animation-hidpi',1728,1056,1.5,0,1,2),
    @('diagnostics',1152,704,1,1,1,3),
    @('diagnostics-narrow',900,640,1,1,1,3),
    @('diagnostics-hidpi',1728,1056,1.5,1,1,3)
)
foreach ($case in $cases) {
    $tab = if ($case.Length -gt 6) { $case[6] } else { 0 }
    $bottom = if ($case.Length -gt 7) { $case[7] } else { 0 }
    & $preview (Join-Path $OutputDir "ethhh-chrome-$($case[0]).png") $case[1] $case[2] $case[3] $case[4] $case[5] $tab $bottom
    if ($LASTEXITCODE -ne 0) { throw "Chrome/routing regression failed: $($case[0])" }
}
Write-Output "PASS all $($cases.Count) off-screen preview cases"
exit 0
