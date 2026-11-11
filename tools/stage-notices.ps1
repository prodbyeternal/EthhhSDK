# Stage notices only; never touches live game files, settings, or backups.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$dist = Join-Path $root 'build\dist'
$notices = @(
    @('THIRD-PARTY-NOTICES.md','THIRD-PARTY-NOTICES.md'),
    @('src\third_party\imgui\LICENSE.txt','licenses\Dear-ImGui.txt'),
    @('src\third_party\minhook\LICENSE.txt','licenses\MinHook.txt'),
    @('src\third_party\lua\LICENSE.txt','licenses\Lua.txt'),
    @('src\third_party\liquid_glass\LICENSE','licenses\Liquid-Glass.txt'),
    @('src\third_party\liquid_glass\THIRD-PARTY-NOTICES.md','licenses\Liquid-Glass-Upstream-Notices.md'),
    @('src\third_party\liquid_glass\DISCLAIMER.md','licenses\Liquid-Glass-Disclaimer.md'),
    @('mods\HalfLifeMovement\LICENSE','licenses\Valve-HLSDK.txt'),
    @('mods\HalfLifeMovement\LICENSE','mods\HalfLifeMovement\LICENSE')
)
foreach ($notice in $notices) {
    $src = Join-Path $root $notice[0]
    $dst = Join-Path $dist $notice[1]
    if (-not (Test-Path $src -PathType Leaf)) { throw "Missing required notice: $src" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
    Copy-Item $src $dst -Force
    if ((Get-FileHash $src -Algorithm SHA256).Hash -ne (Get-FileHash $dst -Algorithm SHA256).Hash) {
        throw "Notice staging mismatch: $dst"
    }
}
Write-Output "PASS staged $($notices.Count) attribution/license files (no installation)"
