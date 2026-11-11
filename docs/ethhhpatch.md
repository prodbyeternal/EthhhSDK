# EthhhPatch — display, camera/FOV, mouse attacks and animation interpolation

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
- optionally provide render-only third-person mouse orbit outside HL mode;
- disable compatibility changes if a later successful Reset no longer matches
  the verified active mode.

Compatibility hooks for culling, HUD pulse, movie fit, camera and entity submissions are installed lazily
only when an Apply transaction begins and are removed after restore/failure.
Inactive EthhhPatch therefore adds no pass-through detour to the hot sphere-cull
path. Projection-matrix validation is sampled once per 30 engine frames rather
than performing protected memory validation on every render.

### Third-person orbit and scripted cameras

The F9 **EthhhPatch > Camera > Third-person mouse orbit outside HL mode** option maintains an orbit
around the exact Ninja, initialized from the native camera's distance and angle.
The target is the Ninja position plus an estimated 1.3 game-unit height. Target
translation interpolates the last two completed ticks with one tick of latency;
raw mouse orientation updates every render. The pitch is bounded and yaw wraps.
This changes camera samples, not animation/entity update frequency or simulation.
The wheel adjusts preferred orbit distance from 2.5 to 15 game units. Sensitivity
and invert-Y are persistent; capture failures retry on subsequent frames rather
than permanently disabling mouse movement. Normal native orbits that keep aiming
at Ninja are not classified as off-target scripted pans.

The renderer does **not** take its displayed pose directly from `Camera+0x80`.
`View_tick` snapshots previous/current positions at `View+0x20/+0x40` and
quaternions at `+0x30/+0x50`. `0x43F290` interpolates those and builds the rendered
matrix at `+0x60`, inverse at `+0xA0`, and product at `+0xE0`. Updating Camera alone
could therefore give CPU culling and the visible viewpoint different poses.

The orbit updates both View endpoints and the camera's world/inverse/product
only inside the verified complete `App_render_step` (`0x43E170`) scope. Installation
runs once, at the primary `0x43E22B` call to `renderer_prepare`, whose tail jump
at `0x449E2A` reaches `View_Render` with return address **`0x43E230`**. Secondary
effect calls return at `0x44EC69`, `0x44F576`, `0x44FA5C` and `0x450953`; they
rebuild transforms using the same installed endpoints and never recapture the
native baseline. This corrects the repeated-capture ownership failure, but the
reported camera flicker is not live-accepted until retested. At the
end of the full frame, including early returns/exception unwinding, it restores:

- camera `[0,0x40)` and `[0x80,0x100)`;
- View `[0x20,0x120)` (pose endpoints and derived render matrices).

Orbit itself leaves lens/projection parameters, player position, animation and
script state native. The separate optional FOV policy is described below.
Replaced objects are not restored through stale pointers. If a script/mod
changes the installed pose during the frame, its edits are preserved and the
orbit disables itself rather than overwriting the new owner.

Movies immediately yield capture through the exact movie Present site. HL's
explicit camera owner always has priority. An in-engine **cutscene flag is not
verified**: native-camera distance, aim, roll, lens-change, cut and sustained
off-target-pan checks yield to suspected scripted shots, including nearby shots.
Each suspicion starts a 1.5-second native-camera hold. After that hold, three
distinct stable simulation samples reacquire gameplay automatically. **Resume
orbit after scripted camera** clears the remaining hold but still requires stable
samples; the button stays available while orbit is enabled but suspended.
Replacing the player/camera/View discards target history while preserving the
active hold deadline and preferred zoom. Every matrix component and its affine,
orthonormal basis are checked before acquisition.
Unchecking orbit switches back to the native camera without a display Reset.
False positives and indistinguishable gameplay-shaped cinematic shots remain
possible; live cutscene acceptance is required, not implied by offline tests.
Orbit transitions are counted individually but synchronous file messages are
coalesced to at most one per second. Render ownership failures expose a diagnostic
mask (1 player, 2 class, 4 handle, 8 camera, 16 View); the existing safety fallback
is retained. Instrumentation alone is not a fix or live acceptance claim.

A five-ray padded camera query is capped at at most 60 batches per second,
retracts immediately for obstructions and eases back outward. It is not a verified
camera hull; it can miss geometry between directions/samples. Failed/unreadable
queries retain the native camera instead of assuming a clear path. The full
exact-Ninja identity/list check runs once per completed tick, not every render.

### Normal-mode FOV

**EthhhPatch > Camera > Override normal-mode FOV** enables the **Normal-mode FOV**
slider, from **30 to 120 vertical degrees** (default tuning 60; override off).
It persists `ethhhpatch.normal_fov_enabled` and `normal_fov_degrees`. It requires
an active display patch, but **does not require mouse orbit to be enabled**.

At the verified primary View call, the SDK temporarily installs the full angle
at `View+0x08/+0x0C`, tangent-half-angle at `+0x10`, and secant-half-angle at
`+0x14`. Original `View_Render` builds/submits projection at `+0x120`; the active
sphere culler reads that same lens. The saved lens `[0x08,0x18)` and derived
projection `[0x120,0x160)` are restored in the full render scope's `finally`,
before the next native tick. **Native `Camera+0x108` is never changed**, so native
simulation and scripted lens-change detection do not see the user override.

F9, focus loss, movies, explicit HL camera ownership, changed identities and
suspected scripted framing retain native/HL lens behavior. FOV-only sampling uses
the same stable-gameplay/cut heuristic as orbit without taking mouse capture.
There is still no verified in-engine cutscene flag: cinematic exclusions require
live testing. A replaced View or foreign lens edit is not overwritten at cleanup;
an ownership/cleanup failure disables further camera/lens overrides.

### Native mouse attacks

**Mouse attacks: M1 slash / M2 spin** is enabled by default and persists as
`ethhhpatch.mouse_attacks`. It is independent of the high-refresh display patch:
Mouse 1 adds the normal slash action, and Mouse 2 adds held spin pressure for the
native mid-air sword-hover behavior. It does not force an attack state, create
extra ticks, or send Windows keyboard input. Keyboard and gamepad controls remain
native. **Release both mouse buttons after startup, F9, focus/owner changes,
disable/re-enable or remapping before the bridge rearms.**

The verified keyboard poll `0x4427A0` updates a 512-byte down/press-edge buffer at
`Input+8` inside a native tick. The current action table begins at `Input+0x240`:
20 actions, two 28-byte descriptors each. Native action pressure at `0x441BB0`
reaches `0x442FE7`, which reads the down byte, producing held pressure rather than
a repeated tap. Mouse presses only add their corresponding down/edge bytes;
existing native edges and simultaneous physical-key holds are preserved.

`KEYBOARD.CFM` is embedded in `Data/general.vol` at `0x3800` (630 bytes), not a
loose settings file. Rows 9 and 7 bind Left Arrow (`0xCB`) and Right Arrow
(`0xCD`). Embedded `PS2.CFM` maps their alternate-bank rows to square/circle
buttons. **Slash/spin names are layout inference**, not recovered named engine
symbols; actual attack/held hover must be confirmed in-game.

The bridge follows live custom bindings but accepts only ungated keyboard
descriptors. It refuses movement/jump/modifier/menu/F-key scans, overlapping
slash/spin keys, and overlap with any other action or keyboard gate in either
bank. Unsupported remaps leave all native controls untouched. It validates the
selected exact-Ninja class/handle/live membership once per tick without allocating
inspector snapshots, rechecks player/keyboard ownership at the poll, respects
native `Input+0x37CC` keyboard suppression, and requires positive bounded time,
focus, closed F9 and an active native-tick scope.

### Camera and entity render interpolation

The engine forces `App+0x28` to 1 before rendering. Around original `View_Render`
(`0x43F290`) only, EthhhPatch substitutes the verified timer's remainder divided
by one fixed tick. The original camera quaternion/position interpolation then
runs, and the saved fraction is restored even during exception unwinding. This
uses previous/current poses with one tick of latency; it does not extrapolate or
run extra simulation. Free-look orbit supplies its own render endpoints instead.
Paused/zero-step and F9 frames keep native poses.

**EthhhPatch > Animation** enables render-only entity root and bone blending.
After a completed native tick and all mod callbacks, the SDK samples a complete
bounded strat list. Eligible owners keep previous/current root and node matrices
in SDK-owned memory. The two verified renderer boundaries are:

| Site | x86 thiscall arguments after `this` |
| --- | --- |
| `0x44A490` | owner, model instance, world matrix |
| `0x449EB0` | mesh list, world matrix, node matrices |

Both return `ret 12`. The node routine iterates mesh `+0x44` and copies matrix
inputs into renderer-owned queue storage (`+0xDFB0`, count `+0x45F70`); it neither
retains those inputs nor advances animation. The SDK substitutes only renderer
arguments, never native root/bone storage. `0x413B30` and `0x4132D0` are never
called by the new render hooks; the latter advances native animation with
`TimeStepF` and builds the completed node array.

Guards check owner/class/handle-manager generation, model/controller, node count,
allocation header and instance identity. Node count at owner `+0x140` must agree
with `model[0]` and controller `+0x24`; matrices begin at owner `+0x13C`. The
16-byte allocation header must be allocated and have a sufficient end boundary
and a non-null heap owner. Limits are 512 entities, 512 nodes/entity and 16,384
total nodes. Mesh chains are limited to 512 links; native sign-extended node
indices outside the array are refused (`0xFFFF` means world-only).

For double-buffered model instances, only the exact existing `+0x128/+0x12C`
pair reversal under unchanged nonzero `+0x144` mode is allowed. This matches the
verified native per-render swap at `0x43E7E6`; replacement instances do not inherit
old endpoints. Root and bones must still match their completed-tick snapshots
at submission, so intervening writes are preserved. Nested submissions lose the
outer substitution scope temporarily and always restore it on exit.

Rotation uses shortest-arc quaternion slerp, with translation and signed scale
interpolation. Sheared, degenerate, non-affine, unreadable and discontinuous poses,
missed ticks, changed owners/allocations and teleports use native rendering.
Both mesh passes are bounded before root substitution. Unsupported paths such
as custom vertex deformers and particles are not claimed as interpolated; actual
live entity coverage and visual smoothness must be checked in-game.

#### Entity performance correction

The previous live build was reported to fall to approximately **20 FPS and
3–11 active simulation Hz** with entity interpolation enabled. That regression
is not considered accepted or resolved merely because offline math tests pass.

Sampling now walks only the complete bounded owner/link list, using reused
8,192-owner storage and a fixed hash set rather than inspector snapshots and
quadratic cycle searches. Batched identity/allocation reads still validate the
same owner, handle generation, class, model/controller, instances and bone bounds.
Readable-region metadata is cached only within a sampling phase or full render
frame; **every copy remains SEH-protected**, and no native bytes are cached by
the reader. General `SafeReadBytes` is unchanged.

Stable pose buffers reuse allocation capacity; invalid new poses are rejected
before creating cache entries. Render validation reuses bounded scratch matrices.
Mesh walks remain capped at 512 links, with linear work and batched index/link
reads. Repeated identical-alpha blends reuse SDK output only after all fresh
identity/root/bone checks succeed. Telemetry publishes once per frame, not under
a mutex for every entity submission.

Optional sampling has a **2 ms soft budget per completed tick**; SDK render
validation/blending has a **2 ms aggregate soft budget per frame**, excluding
the original renderer and Present/pacing. A started bounded operation and its
cleanup can finish beyond the limit; subsequent owners use native poses. An
incomplete/changed/cyclic live list discards all sampled poses. Budget exhaustion
after a complete list prunes unprocessed owners, so stale pairs cannot render.
No simulation debt, tick count, animation updates or engine scheduling is changed.

The Animation tab reports last sample/render SDK milliseconds, page-query counts,
list completeness and cumulative budget stops. Check performance with F9 closed:
the manager deliberately suspends pose sampling while open. These observations
and manual enabled/disabled testing are required before claiming recovered FPS.
Last eligible gameplay timing/query measurements remain visible while F9 is open
instead of being overwritten by paused-frame zeros.

### Redesigned workspace

Launcher, F9 overlay and previews share `DrawManagerLayout`: compact 64-unit
chrome, graphite/mint theme, navigation rail, library metrics and content-sized
mod cards, and a consistent footer. The library scrolls as a whole so tall
loader-error cards remain reachable on narrow displays. The EthhhPatch page has
**Display**, **Camera**, **Animation** and **Diagnostics** tabs. Existing saved
page IDs and accent values remain valid; the mint accent is only the default.
The gameplay **EthhhSDK** watermark is always visible, ignoring the obsolete
`core.watermark=false` setting; corner/detail preferences remain supported.

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

Earlier builds were installed and manually exercised; this performance/camera/input/FOV correction
is **staged only, not installed or live-accepted**. Live acceptance must confirm exact 1080p/4K Reset,
100-Hz game Presents with 60 active ticks/s, edge visibility, HUD containment,
movie fit, pause/focus/load behavior, and successful manual restore. Unsupported
or mismatched modes are refused or rolled back rather than silently accepted.
Camera acceptance must also cover close and distant in-engine cutscenes, movies,
wall proximity, respawn, explicit resume, F9/focus transitions and HL handoff.
Check FOV at 30/60/90/120 degrees with orbit on and off, including projection/culling
at screen edges and native lens restoration after disabling the override. Check
M1 slash, held M2 spin/hover, keyboard/gamepad coexistence, custom keys, and safe
release-to-rearm after F9/alt-tab/respawn.
Animation acceptance must compare enabled/disabled blending at 100 render Hz
while active simulation remains 60 Hz, with visible Ninja/enemy/attachment poses,
pause/load/respawn, teleports and double-buffered models. UI acceptance includes
launcher Start/Exit, F9 navigation, narrow/4K layouts and live Lua/C# interfaces.

## Reproduction

```powershell
.\tests\run-tests.ps1
python .\tools\inspect-ethhhpatch.py --out "$env:LOCALAPPDATA\Temp\opencode\ethhhpatch-static-NEW.json"
.\tests\run-preview-tests.ps1
```

The inspector checks 199 instruction anchors, 59 bounded disassembly slices,
51 explicit render/lifecycle/collision/camera/entity/input edges, PE import/section mapping,
the projection/movie/entity submissions, primary/secondary View callers, native
keyboard action/pressure paths and both Present return sites. Thirty evidence tests
exercise those checks and the fingerprinted embedded control maps. It refuses altered executable/archive
fingerprints, output outside the approved temporary directory, and overwrite.
It never opens a process or modifies executable/archive bytes. Off-screen
preview tests cover the new page without starting or controlling the game.
They also exercise synthetic live-data layouts at standard/narrow/high-DPI
sizes; those screenshots explicitly say **OFF-SCREEN FIXTURE**. Their example
100-Present/60-tick readings are test data, not observed game performance.

`EthhhPatchEntityTests.cpp` tests all-axis and composed quaternion rotations,
reflected/nonuniform scale, zero native writes across repeated renders, instance
swaps, intervening writes, lifetime/allocation/mesh bounds, tick wrap and resource
ceilings, scoped permissions/protected-copy behavior, complete/cyclic/oversize
lists, reused vector storage and deterministic optional-work budgets.
`EthhhPatchEntityPerformanceTests.cpp` exercises the exact production Windows
reader, real page revocation and guard pages, warmed-up allocation counts, and
conservative `SafeReadBytes` versus scoped-reader sampling/render benchmarks.
Timing output is evidence, not a flaky pass threshold or in-engine FPS claim.
The 25 preview cases use the real shared layout and all four tabs,
check balanced ImGui stacks and launcher routing, and exercise the always-on
watermark at all four corners and three detail levels. Camera cases include
640x480, 1080p, 4K, high-DPI and bottom-of-page scrolling.

`EthhhPatchCameraTests.cpp` tests the runtime-shared orbit policy, quaternion
convention and fake-memory restoration behavior. It does not execute the original
engine rendering or prove cutscene detection/camera collision in a live level.
`MouseAttackTests.cpp` tests native-edge preservation, independent held mouse
buttons, safe rearming, custom bindings, flag padding and cross-action conflicts.
`EthhhPatchLensTests.cpp` tests primary-only installation, coupled orbit/lens
restoration, projection/culling lens math, foreign edits and failed-write cleanup.

The latest dated offline verification and staged artifact hashes are in
[the 2026-11-11 entity performance report](verification-entity-performance-2026-11-11.md).
