# EthhhPatch implementation verification — 2026-11-10 23:09

## Outcome

EthhhPatch is no longer observation-only in source. The staged x86 Release build
contains a user-gated, in-game **Apply experimental patch** transaction and a
**Restore previous display** transaction.

The build was **not installed or live-tested**. No game process was launched,
closed, restarted, suspended, selected, or given automated input.

## Implemented runtime path

- Fullscreen preflight requires an exact adapter-reported width, height, and Hz.
- Windowed application establishes and verifies the exact requested client size.
- Current swap-chain parameters and window rectangle are captured for rollback.
- The engine's verified mode request and Reset lifecycle applies the change;
  EthhhPatch does not directly call `IDirect3DDevice9::Reset`.
- Requested Reset parameters must match exactly before compatibility hooks activate.
- Failures/mismatches request rollback; rollback Reset must also match exactly.
- Later successful Resets that change the verified mode disable compatibility work.
- The pre-game launcher can save plans but cannot apply/restore; application is
  restricted to the game window/render thread and requires explicit acknowledgement.

When active:

- exact bytes `7C F2` at `0x43D96C` become reversible `90 90`, reaching the
  original zero-count tick skip while retaining the timer/debt consumer and
  four-tick catch-up cap;
- game Presents are QPC-paced to the selected target if driver Present does not block;
- View aspect is restored after each completed original tick;
- the verified aspect-blind sphere culler uses tested perspective/aspect bounds;
- three checked fixed-4:3 HUD identity projections are horizontally contained;
- the render-driven HUD pulse advances from completed ticks but still draws every render;
- the fixed 640x480 movie surface uses centered uniform fit instead of independent
  X/Y stretching.

## Verification passed

| Check | Result |
| --- | --- |
| Final Release x86 core/proxy/managed host/C# mod build | Pass |
| HL legacy movement/jump/camera math | 23,721 assertions |
| Managed host/mod against fake engine | 86 assertions per run across three scale cases |
| Input/idle learning scorer | 8 assertions |
| Strat identity policy | 33 assertions |
| EthhhPatch cadence/display policy | 24,415 assertions |
| Projection/runtime/culling/HUD/movie policy | 274 assertions |
| Static EthhhPatch evidence | 21 tests; 95 anchors, 33 bounded slices, 23 explicit edges |
| x86 export ABI | 214 required exports; 212 P/Invoke bindings resolved |
| Off-screen UI/routing | 11 profiles; actual trainer parsed by Lua 5.4 without execution |

Synthetic previews remain explicitly marked `OFF-SCREEN FIXTURE`; they are not
live FPS or compatibility evidence.

## Preservation and hashes

- Live executable/header/installed DLL/configuration/save baseline: **10/10 unchanged**.
- Original Data files: **16/16 unchanged**.
- Complete staged/source/evidence manifest:
  `C:\Users\Administrator\AppData\Local\Temp\opencode\ethhhpatch-implementation-evidence-20261110-2309.json`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/dist/dinput8.dll` | 90,624 | `0d0d82080168a8e2b445c80cc32d0ca45973aeb67475b80dd0d83491afa0937e` |
| `build/dist/EthhhSDK.dll` | 1,232,384 | `2bcc693a94f71b25bb765e510e6b9c695739970b51d529be2d959b30b3d6b73d` |
| `build/dist/EthhhSDK.Managed.dll` | 56,832 | `2aa3dda0631f44990af16ffe2aa7f2f8f8e0da21739a6599207cc162239ee96d` |
| `build/dist/mods/HalfLifeMovement/Mod.dll` | 27,648 | `8a5c1aebf95bb43b06172865b59f661626ea74e927c0157b9ab065b9b0ec2b7b` |
| `build/dist/mods/HalfLifeMovement/mod.json` | 456 | `8c6487de965d1a50fcdc05dcdae421312eaf4edacf801a1e249e1428ee930d7c` |

## Live acceptance still required

Offline verification cannot prove rendering compatibility. The first authorized
live pass must verify 1920x1080 at 100 Hz, approximately 100 successful game
Presents/s with 60 active ticks/s and one game-second/real-second, widescreen edge
visibility, HUD containment, movie fit, pause/focus/load behavior, failed-mode
rollback, and manual restore. 4K follows only after the 1080p acceptance pass.

Installation remains deferred until the user confirms the game was manually
closed. It must use `install.ps1` without `-Force`; game restart remains manual.
