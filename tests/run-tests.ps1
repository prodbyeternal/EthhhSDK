[CmdletBinding()]
param([ValidateSet('Release','Debug')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$build = Join-Path $root 'build'
$out = Join-Path $build 'tests\interop'
$dist = Join-Path $build 'dist'
$cmake = (Get-Command cmake).Source
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$csc = Join-Path $vs 'MSBuild\Current\Bin\Roslyn\csc.exe'
foreach ($file in @('EthhhSDK.dll','EthhhSDK.Managed.dll','mods\HalfLifeMovement\Mod.dll')) {
    if (-not (Test-Path (Join-Path $dist $file))) { throw "Build first: missing $file" }
}
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A Win32 -DETH_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw 'Test configure failed.' }
& $cmake --build $build --config $Configuration --target eth_test_native eth_player_learning_tests eth_strat_identity_tests eth_ethhhpatch_policy_tests eth_ethhhpatch_projection_tests eth_ethhhpatch_camera_tests eth_ethhhpatch_interpolation_tests eth_ethhhpatch_entity_tests eth_entity_performance_tests eth_movement_input_tests eth_mouse_attack_tests eth_ethhhpatch_lens_tests eth_sha256_tests eth_camera_relative_heading_tests --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native test shim build failed.' }
Copy-Item (Join-Path $dist 'EthhhSDK.Managed.dll') $out -Force
Copy-Item (Join-Path $dist 'mods\HalfLifeMovement\Mod.dll') $out -Force
$refs = @('mscorlib.dll','System.dll','System.Core.dll') | ForEach-Object { '/reference:"' + (Join-Path "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319" $_) + '"' }
$common = @('/nologo','/platform:x86','/langversion:latest','/optimize+') + $refs
& $csc @common /target:exe "/out:$out\HlMathTests.exe" "$root\tests\HlMathTests.cs" "$root\mods\HalfLifeMovement\src\HlMove.cs" "$root\mods\HalfLifeMovement\src\Vec3.cs"
if ($LASTEXITCODE -ne 0) { throw 'Math tests compile failed.' }
& $csc @common /unsafe /target:exe "/out:$out\HlCollisionTests.exe" "/reference:$out\EthhhSDK.Managed.dll" "$root\tests\HlCollisionTests.cs" "$root\mods\HalfLifeMovement\src\HlCollision.cs" "$root\mods\HalfLifeMovement\src\HlMove.cs" "$root\mods\HalfLifeMovement\src\Vec3.cs"
if ($LASTEXITCODE -ne 0) { throw 'Collision tests compile failed.' }
& $csc @common /target:exe "/out:$out\HlWallTests.exe" "/reference:$out\EthhhSDK.Managed.dll" "$root\tests\HlWallTests.cs" "$root\mods\HalfLifeMovement\src\HlWall.cs" "$root\mods\HalfLifeMovement\src\HlCollision.cs" "$root\mods\HalfLifeMovement\src\HlMove.cs" "$root\mods\HalfLifeMovement\src\Vec3.cs"
if ($LASTEXITCODE -ne 0) { throw 'Wall tests compile failed.' }
& $csc @common /target:library "/out:$out\LifecycleFixture.dll" "/reference:$out\EthhhSDK.Managed.dll" "$root\tests\LifecycleFixture.cs"
if ($LASTEXITCODE -ne 0) { throw 'Fixture compile failed.' }
New-Item -ItemType Directory -Path "$out\fixture","$out\hl" -Force | Out-Null
Set-Content "$out\fixture\mod.json" '{"name":"Lifecycle fixture"}' -Encoding UTF8
Copy-Item "$root\mods\HalfLifeMovement\mod.json" "$out\hl\mod.json" -Force
& $csc @common /target:exe "/out:$out\ManagedInteropTests.exe" "/reference:$out\EthhhSDK.Managed.dll" "/reference:$out\LifecycleFixture.dll" "$root\tests\ManagedInteropTests.cs"
if ($LASTEXITCODE -ne 0) { throw 'Interop tests compile failed.' }
& "$out\HlMathTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'HL math regression failed.' }
& "$out\HlCollisionTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'HL segment collision regression failed.' }
& "$out\HlWallTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'I-Ninja wall-state regression failed.' }
foreach ($scale in @('0.025','0.05','NaN')) {
    & "$out\ManagedInteropTests.exe" $scale
    if ($LASTEXITCODE -ne 0) { throw "Managed interop regression failed (scale $scale)." }
}
& "$out\ManagedInteropTests.exe" 0.025 autohop
if ($LASTEXITCODE -ne 0) { throw 'Managed Auto-hop/jump regression failed.' }
& "$out\ManagedInteropTests.exe" 0.025 live-tuning
if ($LASTEXITCODE -ne 0) { throw 'Managed persisted-tuning jump regression failed.' }
& "$out\ManagedInteropTests.exe" 0.025 current-tuning
if ($LASTEXITCODE -ne 0) { throw 'Managed current-tuning movement/jump regression failed.' }
foreach ($mode in @('additive','camera-only')) {
    & "$out\ManagedInteropTests.exe" 0.025 $mode
    if ($LASTEXITCODE -ne 0) { throw "Managed input-ownership regression failed ($mode)." }
}
& "$out\eth_player_learning_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Player learning regression failed.' }
& "$out\eth_strat_identity_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Strat identity regression failed.' }
& "$out\eth_movement_input_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Native movement input regression failed.' }
& "$out\eth_camera_relative_heading_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Scoped camera-relative heading/Win32 ABI regression failed.' }
& "$out\eth_mouse_attack_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Mouse attack hold/edge regression failed.' }
& "$out\eth_ethhhpatch_lens_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Primary camera pass/FOV regression failed.' }
& "$out\eth_sha256_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Native SHA-256 file hashing regression failed.' }
& "$out\eth_ethhhpatch_policy_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'EthhhPatch policy regression failed.' }
& "$out\eth_ethhhpatch_projection_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'EthhhPatch projection regression failed.' }
& "$out\eth_ethhhpatch_camera_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'EthhhPatch camera regression failed.' }
& "$out\eth_ethhhpatch_interpolation_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'EthhhPatch interpolation regression failed.' }
& "$out\eth_ethhhpatch_entity_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'EthhhPatch entity-animation regression failed.' }
& "$out\eth_entity_performance_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Production entity reader safety/performance regression failed.' }
& python "$root\tests\test_ethhhpatch_evidence.py"
if ($LASTEXITCODE -ne 0) { throw 'Static EthhhPatch evidence regression failed.' }
& python "$root\tests\test_hotpath_guards.py"
if ($LASTEXITCODE -ne 0) { throw 'Hot-path performance guard regression failed.' }
& python "$root\tests\check-abi.py" $root
if ($LASTEXITCODE -ne 0) { throw 'Export ABI regression failed.' }
Write-Output 'PASS complete offline regression suite (exit 0)'
exit 0
