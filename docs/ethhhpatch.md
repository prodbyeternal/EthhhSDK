# EthhhPatch — experimental resolution and high-refresh patch

## Current behavior

The pre-game and F9 managers include an **EthhhPatch** page for 1080p, 4K or
monitor-size targets, fullscreen/windowed selection, and independent render Hz
(0 selects the reported monitor refresh). **Save future plan** persists the
plan without applying it. Actual application is available only from the in-game
F9 manager after an explicit experimental acknowledgement.

Application preflights an exact fullscreen adapter mode or establishes the exact
windowed client size, captures rollback state, and asks the game's own mode/Reset
lifecycle to apply it. The Reset parameters must exactly match the request before
the render split, aspect/culling, HUD, movie, and pacing policies activate. Any
failure or mismatch requests rollback; **Restore previous display** is available
while active. The pre-game launcher cannot apply or restore a display transaction.

The new device Present detour preserves all arguments and the original HRESULT.
It observes only the verified game return site `0x44A485` and movie return site
`0x496CB6`, for the game device. Successful game presents are counted separately
from completed `Fn_Tick` calls and active, positive-time-step ticks. Simulation
time advance is summed independently; paused ticks are not active simulation.
Movies reset the sampling window and are never counted as game FPS.

Backbuffer size, viewport, present parameters and engine aspect are read on the
render thread using transient COM references. Device Reset invalidates samples.
The watermark adds measured game-present/active-tick rates when available;
ImGui FPS and the OS-reported monitor refresh remain separately labeled.
Reset handling is restricted to the game device and records attempts,
success/failure, HRESULT and a pre-call copy of requested presentation
parameters. EthhhPatch does not call `IDirect3DDevice9::Reset` directly: it uses
the engine's pending-mode path and verifies the resulting Reset.

A SetTransform detour observes **only** successful main-world
projection submissions at return address `0x43F2DE`, state `D3DTS_PROJECTION`,
for the game device. It checks that the matrix argument is the current
`View+0x120`, rechecks the View pointer/parameters, and inspects the canonical
left-handed perspective matrix without modifying it. The page reports actual
matrix aspect, vertical/horizontal FOV, estimated near/far planes and
matrix-versus-source errors. HUD/orthographic/ImGui submissions are excluded.
Reads remain non-atomic; clip-plane estimates inherit matrix float precision.
While active, three checked fixed-4:3 identity-projection sites are horizontally
contained around NDC center. World, orthographic, movie, and ImGui matrices are
not broadly guessed or rewritten.

Cadence diagnostics distinguish new engine frames with **no completed tick**,
repeated Presents of the same engine frame, and multi-tick catch-up frames.
Paused zero-dt ticks still count as completed ticks, not zero-tick renders.
Movies/unreadable frame counters break the comparison baseline; Reset clears
the counters. Timing and projection samples expire after 2.5 seconds, so an
alt-tab/device pause cannot leave old FPS displayed as a live measurement.

## Verified static timing/display facts

For executable SHA-256
`49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0`:

| Site | Meaning |
| --- | --- |
| `0x43D962` | ask timer `0x401160` for due ticks |
| `0x43D969..0x43D96C` | original wait until due ticks >= 1; active patch reversibly NOPs only the back-branch |
| `0x43D970` | consume actual whole-tick timer debt via `0x401110` |
| `0x43D987` | cap catch-up to four ticks |
| `0x43D99B`, `0x43D9B8` | original first/extra simulation ticks |
| `0x43D9B0` | special strat-frame-stamp preparation before extra ticks |
| `0x43D9C4` | render step after simulation ticks |
| `0x43EFE4` | write View aspect = 4/3 every tick |
| `0x452300` | configure requested wrapper dimensions/fullscreen refresh |
| `0x452360` | build present parameters before creation/reset |
| `0x45234E` | post `WM_SIZE` after marking wrapper+0x46 mode/reset pending |
| `0x456E90` | refresh window dimensions for `WM_SIZE` |
| `0x44AFD0` | render-side cooperative-state / teardown / Reset / rebuild lifecycle |
| `0x452420` | wrapper Reset; blocked while inactive/minimized via wrapper+0x45 |
| `0x459320` | script-selected display mode requests 60 Hz |
| `0x449E03` | render preparation clears scene submission queues |
| `0x449E2A` | render preparation enters View matrix builder `0x43F290` |
| `0x43F2CD` | builds LH perspective using View+0 aspect and View+0x0C vertical FOV |
| `0x43F2D8` | submits View+0x120 through SetTransform(PROJECTION) |
| `0x449420` | one CPU sphere culling path; no View aspect read |
| `0x40C859` | render-flagged shared UI path calls pulse updater `0x4483B0` |
| `0x4483B9` | pulse phase advances by constant 0.1 per eligible render |
| `0x499FCC` | create fixed 640x480 movie render surface |
| `0x49A2C1`, `0x49A2DF` | original movie X/Y scales use backbuffer/640 and backbuffer/480 independently |
| `0x49A405` | movie draw dispatch hooked for centered uniform fit while active |

Faking a due count of one lets the loop execute a simulation tick when no real
tick is due. The timer's separate debt-consumer does not repair that speedup.
Similarly, changing backbuffer dimensions or writing aspect after a tick does
not establish correct projection/culling, HUD layout or movie presentation.

### Newly verified blockers

The primary matrix builder `0x45ED0F` calculates `yScale=cot(verticalFov/2)`
and `xScale=yScale/aspect`, with near plane `0.01` and far plane `View+0x04`.
This establishes the **main world perspective's** vertical-FOV convention;
it does not establish every shader, HUD or movie projection convention.

The sphere test `0x449420` uses the camera inverse-view matrix at `+0xC0`, a
far-distance check and a side limit `cameraZ + tanHalfFov * sphereRadius`, with
the same bound for X and Y and no aspect load. For a zero-radius point and
identity camera, a 90-degree vertical / 16:9 projection includes `(15,0,10)`,
but that CPU test rejects it because `15 > 10`. The offline tests reproduce this
counterexample. The active compatibility hook replaces this routine with a
normalized sphere/frustum test using vertical FOV, actual aspect, near/far bounds
and horizontal/vertical sphere-plane margins. Other visibility routines remain
an acceptance-test risk rather than a claim.

Shared UI `0x40C620` receives flag 0 from Tick and flag 1 from RenderFrame.
In eligible mode-5 states, its render branch calls `0x4483B0`, which adds 0.1
to phase `+0xE8` before drawing the pulse. Calling that path at 100 rather than
60 Hz advances its phase faster. Skipping the entire helper would also skip its
drawing. The active hook pre-adjusts only phase `+0xE8` so the original helper
still draws every render while phase advances according to completed simulation
ticks, including zero-tick and bounded catch-up frames.

RenderStep also advances the engine frame count and clears/rebuilds submission
queues. EthhhPatch therefore enables real zero-tick RenderStep calls rather than
repeating Present. Target-Hz QPC pacing limits the loop if driver Present does
not block at the selected refresh.

### Display and Reset lifecycle

`0x452300` stores the requested width, height, fullscreen refresh and inverse
`Windowed` flag, marks wrapper byte `+0x46`, then calls imported `PostMessageA`
with `WM_SIZE`. It does **not** directly resize the Win32 window. The member
WndProc handles `WM_SIZE` by calling `0x456E90` and marking `+0x46` again.

In windowed mode, `0x456E90` calls `GetWindowInfo` and derives width/height from
the current client rectangle. Consequently, a future windowed-resolution path
must deliberately establish the desired client size; merely calling the mode
request with width/height does not do so. In fullscreen mode, the same helper
copies the requested wrapper dimensions into the window object.

Before CreateDevice or Reset, `0x452360` rebuilds `D3DPRESENT_PARAMETERS`.
Windowed mode uses the current window dimensions, refresh 0 and presentation
interval 0 (`D3DPRESENT_INTERVAL_DEFAULT`). Fullscreen mode calls `0x452FF0`,
which only considers enumerated modes whose width, height and refresh do not
exceed the request, then uses interval 1 (`D3DPRESENT_INTERVAL_ONE`). This is a
bounded fallback selection, not proof that the requested mode is applied exactly.
The constructor's initial request is 640x480 at 60 Hz.

Every RenderStep enters `0x44AFD0`. A pending mode change makes the status check
fail, causing renderer resource teardown, wrapper Reset, and renderer rebuild
only after Reset succeeds. While inactive/minimized, wrapper byte `+0x45`
blocks the Reset attempt. EthhhPatch uses this lifecycle transactionally and
does not call the device Reset method itself.

### Runtime split and compatibility policies

The original App loop already tests the queried due count and skips `Fn_Tick`
when it is zero at `0x43D993..0x43D995`. The only unreachable part was caused by
the back-branch at `0x43D96C`. On activation, exact original bytes `7C F2` are
reversibly replaced by `90 90`. The original query, timer debt consumer,
four-tick catch-up cap, callback queue, RenderStep, and Present remain in place.
The debt consumer records sub-tick elapsed time even when zero whole ticks are
due, so simulation is still advanced only through real fixed-60 tick calls.

The active policies also:

- rewrite View aspect after each completed original tick;
- replace the verified aspect-blind sphere culler with the tested perspective test;
- contain checked fixed-4:3 HUD identity projections horizontally;
- advance the verified HUD pulse from completed ticks while drawing every render;
- replace independent 640x480 movie stretching with centered uniform fit;
- pace game Presents to the exact selected/monitor target;
- disable compatibility changes if a later successful Reset no longer matches
  the verified active mode.

Compatibility hooks for culling, HUD pulse, and movie fit are installed lazily
only when an Apply transaction begins and are removed after restore/failure.
Inactive EthhhPatch therefore adds no pass-through detour to the hot sphere-cull
path. Projection-matrix validation is sampled once per 30 engine frames rather
than performing protected memory validation on every render.

## Policy tests and live acceptance

`src/core/ethhhpatch_policy.h` validates display targets, computes aspect/FOV
math previews, and contains a **test-only-use fixed-60-Hz scheduler prototype**.
At 100 render Hz it permits zero-tick render frames and produces 600 simulation
ticks over ten seconds; the same count is tested at 30/60/120/144/165/240/360 Hz.
Stalls cap catch-up at four ticks and explicitly drop excess whole-tick debt.
This debt policy is a prototype design choice, not a change to the game timer.

The runtime uses the game's original timer rather than instantiating the offline
scheduler class. The prototype remains a deterministic oracle for 30..360-Hz
cadence and debt policy.

The patch is implemented and user-gated, but has **not yet been installed or
live-accepted in this build**. Live acceptance must confirm exact 1080p/4K Reset,
100-Hz game Presents with 60 active ticks/s, edge visibility, HUD containment,
movie fit, pause/focus/load behavior, and successful manual restore. Unsupported
or mismatched modes are refused or rolled back rather than silently accepted.

## Reproduction

```powershell
.\tests\run-tests.ps1
python .\tools\inspect-ethhhpatch.py --out "$env:LOCALAPPDATA\Temp\opencode\ethhhpatch-static-NEW.json"
.\tests\run-preview-tests.ps1
```

The inspector checks 95 instruction anchors, thirty-three bounded disassembly
slices, twenty-three explicit render/lifecycle edges, PE import/section mapping,
the projection/movie submissions and both Present return sites. Twenty-one evidence tests
exercise those checks. It refuses altered executable
fingerprints, output outside the approved temporary directory, and overwrite.
It never opens a process or modifies executable/archive bytes. Off-screen
preview tests cover the new page without starting or controlling the game.
They also exercise synthetic live-data layouts at standard/narrow/high-DPI
sizes; those screenshots explicitly say **OFF-SCREEN FIXTURE**. Their example
100-Present/60-tick readings are test data, not observed game performance.
