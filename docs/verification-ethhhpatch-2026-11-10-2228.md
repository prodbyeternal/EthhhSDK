# EthhhPatch display-lifecycle observation verification — 2026-11-10 22:28

## Outcome

The x86 Release build passed after extending EthhhPatch with read-only D3D9
Reset telemetry and checked static evidence for the game's display-mode,
window-dimension, lost-device, Reset, and renderer-rebuild paths.

The result remains staged under `build/dist` and was **not installed**. No game
process was launched, closed, restarted, suspended, selected, or given automated
input. EthhhPatch still does not request a mode, resize a window, force Reset,
change projection/culling, or unlock rendering.

## New verified lifecycle facts

- `0x452300` stores a mode request, marks wrapper byte `+0x46`, and imports
  `PostMessageA` to post `WM_SIZE`; it does not resize the window directly.
- Windowed presentation dimensions come from the current client rectangle via
  `GetWindowInfo`. A future 1080p/4K windowed path must deliberately establish
  the desired client size.
- Fullscreen presentation parameters use an enumerated mode bounded by requested
  width, height, and refresh. The request is therefore not an exact-mode guarantee.
- The initial wrapper request is 640x480 at 60 Hz.
- Every RenderStep enters the cooperative-state lifecycle at `0x44AFD0`. A
  pending change releases renderer resources, calls wrapper Reset at `0x452420`,
  and rebuilds resources only on success.
- Inactive/minimized wrapper byte `+0x45` blocks Reset attempts.
- Runtime telemetry observes only game-device Reset calls and records attempt,
  success/failure, HRESULT, and pre-call presentation parameters. It never calls
  or retries Reset itself.

## Verification

| Check | Result |
| --- | --- |
| Final Release x86 core/proxy/managed host/C# mod build | Pass |
| HL legacy movement/jump/camera math | 23,721 assertions |
| Managed host/mod against fake engine | 86 assertions per run across three scale cases |
| Input/idle learning scorer | 8 assertions |
| Strat identity policy | 33 assertions |
| EthhhPatch fixed-60 policy prototype | 24,412 assertions; not installed in `App_Run` |
| Projection/cadence policy | 246 assertions; offline, no widescreen repair |
| Static EthhhPatch evidence | 16 tests; 72 anchors, 26 bounded slices, 20 explicit edges |
| x86 export ABI | 214 required exports; 212 P/Invoke bindings resolved |
| Off-screen UI/routing | 11 profiles; actual trainer parsed by Lua 5.4 without execution |

The standard, narrow, and high-DPI synthetic layouts explicitly display
`OFF-SCREEN FIXTURE`; their readings are not live performance evidence.

## Preservation and hashes

- Live executable/header/installed DLL/configuration/save baseline: **10/10 unchanged**.
- Original Data files: **16/16 unchanged**.
- Full staged/source/evidence hash manifest:
  `C:\Users\Administrator\AppData\Local\Temp\opencode\ethhhpatch-final-evidence-20261110-2228.json`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/dist/dinput8.dll` | 90,624 | `0d0d82080168a8e2b445c80cc32d0ca45973aeb67475b80dd0d83491afa0937e` |
| `build/dist/EthhhSDK.dll` | 1,217,024 | `e58b51feb3acb5446c3a4f784aca9d347a6f7e67b67afe2797094cb55be2b7a9` |
| `build/dist/EthhhSDK.Managed.dll` | 56,832 | `ebcbd3d2282d9714315f79a34649d25465da8e305d80af117cea1e4bda2171bc` |
| `build/dist/mods/HalfLifeMovement/Mod.dll` | 27,648 | `f283003938e73849c325e5af9778a771cfefa87a354ba9f4e4d0aa2d2f134483` |
| `build/dist/mods/HalfLifeMovement/mod.json` | 456 | `8c6487de965d1a50fcdc05dcdae421312eaf4edacf801a1e249e1428ee930d7c` |

## Remaining gates

Safe display application still requires deliberate client sizing, exact-mode and
rollback policy, depth/render-target verification, aspect-sensitive culling
repair, HUD/movie scaling, render-driven update separation, and the fixed-60-Hz
simulation/render split. Installation remains deferred until the user confirms
manual game closure; it must use `install.ps1` without `-Force`.
