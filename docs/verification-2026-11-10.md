# Tracking/scale diagnostic build — 2026-11-10

## Outcome

Release x86 build installed at 19:54 local time without `-Force`, while no
I-Ninja process was running. No game launch, restart, shutdown, or input
automation was performed. Previous DLLs are preserved in
`_backup/20261110-195410`.

SHA-256 comparison confirmed that six existing executable/settings/save files
were unchanged by installation. All four deployed DLLs match staged outputs.
The test-only fake ABI DLL remains in `build/tests/interop`, never deployed.

## Tests passed

| Check | Result |
| --- | --- |
| Release native core / proxy / managed host / C# mod build | Pass |
| HL legacy movement/jump/camera math | 23,721 assertions |
| Real managed host and compiled HL mod, fake engine | 59 assertions per run; three runs: scales 0.025, 0.05 and NaN fallback |
| Input/idle learning scorer | 8 assertions |
| x86 native export ABI | 211 required exports, 209 P/Invoke bindings resolved |
| Off-screen chrome / launcher routing | Five profiles passed |

Preview profiles: `1152x704 @1.0`, `900x640 @1.0`, `1728x1056 @1.5`, and
long-title/description/error fixtures at both standard and narrow sizes.
Card content stays unclipped, and the stress error card is reachable by
scrolling. Synthetic ImGui activation verifies real Start, Exit and close
controls; routing checks cover Enter/Escape typing guards, Alt-F4, WM_CLOSE,
conflicting requests, balanced chrome stacks and no teardown WM_QUIT.
These use a hidden window only and never invoke `launcher::Run` or the game gate.

Managed coverage includes estimated unit conversion and eye height, anchored
jump at zero/nonzero takeoff heights, holding jump without repeated impulses,
zero/non-finite dt, pause, F3, disable/re-enable, address/class changes, lost
player, failed writes, FOV restoration and capture cleanup.

## Deployed artifacts

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `<game>/dinput8.dll` | 90,624 | `0D0D82080168A8E2B445C80CC32D0CA45973AEB67475B80DD0D83491AFA0937E` |
| `<game>/EthhhSDK.dll` | 1,183,744 | `BA257552D8115195305CCF450AEC2937D8A65086FE5FFB6D3B1CA086DB3C0350` |
| `EthhhSDK.Managed.dll` | 55,808 | `1F07923B8DA4A79969296AD125943FB4FE4D6C499473B1A0B758D7DEA4CF4522` |
| `mods/HalfLifeMovement/Mod.dll` | 26,624 | `8BA3F841DDBFBDD374C5D9184E452460DF1661CB72CADDB97FFB4ACEF9A35E15` |

## Not established

Player identity, model synchronization, world units/axes, grounded state, and
live collision behavior remain unverified. Successful isolated position writes
are not evidence that the visible ninja follows them. Learn provides a
shortlist, not a player-ID proof, and never automatically pins a winner.

The default scale `0.025` remains an estimate. The expected default jump apex is
approximately `45 * 0.025 = 1.125` game units, not a measured ninja jump height.
See [manual verification](hl-verification.md) before further movement work.

EthhPatch widescreen/high-refresh integration is still deferred. Final Valve
agreement text is pending; no distribution-ready license package is claimed.

## Reproduce

```powershell
.\build.ps1 -Reconfigure
.\tests\run-tests.ps1
.\tests\run-preview-tests.ps1
```
