# Pinned-player / EthhhPatch hot-path optimization — 2026-11-10 23:54

## Outcome

The reported lag spike had three concrete software causes. The staged Release
build removes them and passes the complete regression suite.

No game process was launched, stopped, restarted, or given automated input.
`install.ps1` was not run.

## Changes

### Pinned player and HL mode

- Manual mode no longer refreshes/sorts the full strat candidate list every
  tick. The previous empty-candidate condition accidentally bypassed its 250 ms
  throttle and caused a complete scan every tick after pinning.
- Manual validity is cached and periodically revalidated against class, complete
  self handle/generation, handle slot, and live-list membership.
- `ReadStratIdentity` no longer creates a full `StratView` snapshot for every
  entity. It performs a lightweight next-pointer membership walk and full
  validation only for the requested strat/owner.
- HL ImGui no longer resolves the pinned class name through an identity scan on
  every rendered UI frame.
- Camera/player writes to ordinary writable game data no longer call
  `VirtualProtect` and `FlushInstructionCache`. Those operations remain reserved
  for protected executable patches; data writes remain guarded with SEH.

### EthhhPatch

- Sphere-culler, HUD-pulse, and movie-fit hooks are installed lazily only when
  an Apply transaction starts.
- Compatibility hooks are removed after restore, rollback failure, or a later
  incompatible Reset. Inactive EthhhPatch therefore has no hot culler detour.
- Projection matrix diagnostics run once per 30 engine frames rather than doing
  multiple protected reads and locking on every world projection submission.
- The active replacement culler avoids redundant per-matrix finite-value loops;
  computed camera-space values remain finite-checked by the policy function.

### HL camera defaults

- Eye height default and invalid-value fallback: **69 HL units**.
- Forward offset default and invalid-value fallback: **24 HL units**.
- Existing persisted settings remain authoritative. The live settings already
  contained `eye_height=69` and `forward_offset=24` and were not rewritten.

## Verification

| Check | Result |
| --- | --- |
| Final Release x86 build | Pass |
| HL legacy math | 23,721 assertions |
| Managed HL/host interop | 87 assertions per run across three scale cases |
| Player learning | 8 assertions |
| Strat identity | 33 assertions |
| EthhhPatch policy | 24,415 assertions |
| Projection/runtime policy | 274 assertions |
| Static EthhhPatch evidence | 21 tests |
| Hot-path regression guards | 6 tests |
| ABI | 214 exports / 212 P/Invoke bindings |
| Off-screen UI | 11 profiles passed |

Hot-path evidence and staged/source hashes:
`C:\Users\Administrator\AppData\Local\Temp\opencode\ethhh-hotpath-evidence-20261110-2354.json`.

Original Data preservation remained **16/16 unchanged**. The original game
executable and `ininja-offsets.h` also remained unchanged.

## Concurrent live files

The original ten-file live baseline was **7/10 unchanged** at the first final
snapshot. The differing files were `EthhhSDK.dll`, `config/EthhhSDK.ini`, and
`config/mod_settings.ini`, consistent with the ongoing manual live test rather
than this build command. They were preserved.

The live files changed again during verification: the parent `EthhhSDK.dll`
became byte-identical to the optimized staged core, while the live managed host
and HL `Mod.dll` remained older than their staged counterparts. No copy/install
operation was performed here. The read-only point-in-time record is:
`C:\Users\Administrator\AppData\Local\Temp\opencode\ethhh-concurrent-live-snapshot-20261110-2357.json`.

Because replacing DLLs on disk does not update modules already loaded by a
running process, performance acceptance still requires a consistent deployment
after manual game closure and then a manual restart.
