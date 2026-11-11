# EthhhPatch observation build verification — 2026-11-10 22:16

## Outcome

The final x86 Release build completed successfully and remains staged under
`build/dist`. It was **not installed**. No game process was launched, closed,
restarted, suspended, selected, or given automated input.

This build adds read-only main-world projection and render-cadence diagnostics.
It still does **not** apply a resolution, aspect, culling, presentation-rate, or
fixed-simulation scheduler patch. Saving an EthhhPatch plan remains planning
only and does not resize or reset the device.

## Implemented diagnostics

- Observes only successful game-device `D3DTS_PROJECTION` submissions returning
  to verified world call site `0x43F2DE`.
- Rechecks the current View pointer/source parameters and accepts only the
  canonical left-handed perspective matrix at `View+0x120`.
- Reports submitted aspect, vertical/horizontal FOV, estimated clip planes,
  source/matrix errors, and stale-sample state without changing matrix bytes.
- Separates new zero-tick engine frames, repeated Presents of one engine frame,
  and multi-tick catch-up frames. Paused completed ticks are not misclassified.
- Expires projection and timing observations after 2.5 seconds.
- Documents two verified blockers: the aspect-blind CPU sphere culler at
  `0x449420` and a mode-5 HUD pulse that increments by `0.1` per render.

## Tests passed

| Check | Result |
| --- | --- |
| Final Release x86 core/proxy/managed host/C# mod build | Pass |
| HL legacy movement/jump/camera math | 23,721 assertions |
| Managed host/mod against fake engine | 86 assertions per run; 0.025, 0.05, and NaN-fallback runs |
| Input/idle learning scorer | 8 assertions |
| Strat identity policy | 33 assertions |
| EthhhPatch fixed-60 policy prototype | 24,412 assertions; prototype not installed in `App_Run` |
| Projection/cadence policy | 246 assertions; offline, no widescreen repair applied |
| Static EthhhPatch evidence | 12 tests; 40 anchors, 20 bounded slices, 10 explicit render edges |
| ASL/connection research regressions | 29 tests |
| x86 export ABI | 214 required exports; 212 P/Invoke bindings resolved |
| Off-screen UI/routing | 11 profiles passed |
| Actual trainer Lua source | Parsed successfully with vendored Lua 5.4; not executed |

The synthetic live-data profiles cover standard, narrow, and high-DPI layouts.
They display `OFF-SCREEN FIXTURE` above the scroll region. Their 100-Present /
60-tick values are layout fixtures and are not evidence of an FPS unlock.

## Preservation

- Live executable/header/installed DLLs/configuration/save baseline: **10/10
  unchanged**.
- Original `Data` archives and `INinja.dat`: **16/16 unchanged**.
- `ininja-offsets.h` remains unchanged at SHA-256
  `d1b2456255b67a3890f26ed42c84cdcdcea502eb5ad3ddd2ca65405e4496394f`.
- Complete staged/source/evidence and preservation records are stored in
  `C:\Users\Administrator\AppData\Local\Temp\opencode\ethhhpatch-final-evidence-20261110-2216.json`.

## Staged artifact SHA-256

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/dist/dinput8.dll` | 90,624 | `0d0d82080168a8e2b445c80cc32d0ca45973aeb67475b80dd0d83491afa0937e` |
| `build/dist/EthhhSDK.dll` | 1,216,512 | `f1d5ec21bd10e600dd2eeeec11a9d53938f12236cf685b809c69d221a6ce2239` |
| `build/dist/EthhhSDK.Managed.dll` | 56,832 | `d31b8b66458f3ed7c5cd539a74f1b621c3b5437e67fb22fa10e26b563710ca5a` |
| `build/dist/mods/HalfLifeMovement/Mod.dll` | 27,648 | `038dda2e5c6d660b0ff6546e1f1e8f16ff02e49441fdb6862f6ddf2bd83ac83e` |
| `build/dist/mods/HalfLifeMovement/mod.json` | 456 | `8c6487de965d1a50fcdc05dcdae421312eaf4edacf801a1e249e1428ee930d7c` |

## Remaining gates

Before enabling 1080p/4K/high-refresh application, EthhhPatch still needs a
verified correction for aspect-sensitive CPU visibility, separation of
render-driven state updates from drawing, remaining HUD/movie scaling traces,
device Reset/lost-device policy, and a safe render/simulation scheduler split.
Repeating Present alone does not rebuild the scene at monitor refresh.

Installation remains deferred until the user confirms the game was manually
closed. Installation must use `install.ps1` without `-Force`; restart remains
manual.
