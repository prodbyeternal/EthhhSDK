# Verification — native camera-relative movement (2026-11-11)

## Outcome and limits

The final **Release / Win32** core, managed host and HalfLifeMovement mod are
recorded as a **19:22 staged build snapshot, not live-accepted**. The Release build and
complete offline suite genuinely exited **0**. The final shared-manager preview
suite also exited **0** after the Camera movement explanation/counters were added.

This continuation performed no installation, game launch, termination, restart,
game-process inspection, commit or reset. Deployment requires fresh confirmation
that the user has manually closed the game. Never use `install.ps1 -Force`.

**Later file changes:** during the final integrity check, the staged and installed
core/host/mod were found replaced with matching newer hashes, with file times
**19:24:41 / 19:24:49 / 19:24:54**. A new `_backup/20261111-192502` also appeared.
The SDK tuning file subsequently changed at **19:26:45** (font scale 1.1 and
watermark corner 1). These changes were outside this continuation's commands;
their origin/authorization has not been established. They were not overwritten
or reverted. The 19:22 artifact/source/evidence snapshot remains historical.

The 14 recorded implementation/test/tool source hashes were still unchanged at
the integrity check; the newer staged core separately passed the read-only
**219-export / 217-P/Invoke** check. The complete suite's exit 0 applies to the
earlier build snapshot, not proof of these newer binaries' build settings or live
behavior. Further deployment is paused pending the user's clarification.

Direction, camera/cutscene behavior, M1 slash/held M2 spin-hover, normal-mode FOV,
death/traversal/HL handoff and actual FPS remain live-unaccepted. The reported
**20 FPS / 3–11 active simulation Hz** entity regression is not declared resolved.
Final Valve agreement/attribution packaging also remains pending.

## Movement contract reviewed

- Native axis conversion, dead zone and magnitude remain unchanged. Ninja's
  script reads heading and adds the script Camera strat's yaw.
- The shared native getter always runs once; its EAX result is retained. Only
  the verified heading call can replace one VM scratch float with
  `native heading + last rendered orbit yaw * 4096/(2*pi) - native Camera yaw`.
- Eligibility exists only around one original native tick, using a successfully
  installed primary orbit. A stale sample expires after four catch-up ticks or
  0.25 seconds. Native-tick cleanup uses `__finally` before SDK/mod callbacks.
- Caller, exact five-word ASL sequence, register-pool/output address, player and
  Camera-owner generations, Camera target and render Camera/View identity are
  verified. Unreadable/replaced state retains the original result. The write is
  exactly four bytes, without a page-protection change.
- F9/focus/pause, movie/scripted-camera holds, restoration failure, HL camera
  ownership/WASD lease and Ctrl traversal priority yield to native control.
  Ctrl release has a bounded 30-tick grace. Fresh orbit rendering is required
  after ownership invalidation.
- Raw axes, actions, movement speed, transforms, animation and traversal state
  are not modified by this hook. Unrelated getter reads do no identity-list walk,
  allocation, lock or logging. Full Camera identification is cached under stable
  class/handle tokens.

There is no verified native death/cutscene/rail/chain/traversal flag. Native script
state, Ctrl grace and orbit heuristics are safeguards, not full state coverage.
Exact addresses and behavior are documented in [ethhhpatch.md](ethhhpatch.md).

## Final offline results

| Check | Result |
| --- | --- |
| Scoped native heading and actual production x86 thunk ABI | 150,929 assertions |
| Mouse attacks, native holds/edges/rearming/bindings | 1,004 assertions |
| Primary render gate and coupled pose/lens restoration | 1,418 assertions |
| Orbit/restoration policy | 14,208 assertions |
| Native WASD lease/byte ranges | 41,876 assertions |
| Entity interpolation/lifetime/budget/buffer policy | 130,991 assertions |
| Static evidence/refusal tests | 32 tests |
| Hot-path guards | 21 tests |
| SDK x86 exports / managed P/Invoke | 219 / 217 resolved |
| Shared manager previews after final Camera text | All 25 cases passed |
| Additional Camera bottom-scroll previews | 3 cases passed |

HL math/collision/wall-state, managed interop/lifecycle, Ninja identity/facing,
render cadence/projection and BCrypt regressions also passed. The movement suite
checks yaw signs/wrapping, eligibility/handoff, owner and ASL mutations, protected
read/write refusal, unchanged non-scratch bytes and thunk stack/nonvolatile/EAX
preservation. None of these tests executes live Ninja gameplay.

Fresh native evidence checks **235 anchors / 73 bounded slices / 57 edges**.
Decoded-ASL evidence verifies the fingerprinted Ninja heading sequence and copy
refusal without process access. Decoded copyrighted packages remain outside the SDK.
The older 233-anchor/72-slice evidence is retained, not overwritten.

Visual inspection covered 640×480 top/bottom, 900×640 bottom, 1080p top/bottom and
4K HiDPI top/bottom. Movement counters and warnings wrap and remain reachable by
scrolling; narrow-page scroll clipping is intentional, not missing content.
These off-screen layouts are not live F9/display/FPS observations.

### Synthetic entity-reader measurements

The final suite measured conservative → scoped sample time **22.4327 → 1.1301 ms**
and SDK render validation **25.4638 → 0.1606 ms**, with page queries **2,178 → 2**
per sample and **2,560 → 3** per frame. The production-reader test passed real
page-revocation/guard-page checks and **zero warmed-up heap allocations**.
These are synthetic reader measurements, not engine FPS or simulation-rate proof.

## Preservation and artifacts

Final read-only comparison checked **67 recorded files: 61 unchanged, 6 retained
differences**. Original executable, Data archives, `ininja-offsets.h`, saves,
original settings, logs and recorded manifests remain intact. Differences are
the historical installed core/host/mod, retained user `config/EthhhSDK.ini`, the
updated behavior document and the continued `tests/chrome_preview.cpp` work.
Nothing was reverted to an older baseline.
The later comparison is retained separately as
`%LOCALAPPDATA%/Temp/opencode/ethhh-camera-relative-later-observation-20261111.json`;
it records the newer binaries/tuning without replacing the historical evidence.

At 19:22, installed core/host/mod timestamps were **17:23:02 / 17:17:53 / 17:17:58**;
tested staged outputs were **19:13:53 / 19:14:00 / 19:14:06**. Retained tuning includes
1080p fullscreen, 100 Hz, orbit on, normal FOV at 80°, entity interpolation on and
accent `6C8CFF`. It was not replaced. Legacy `watermark=0` remains in the file;
the implementation still always renders the EthhhSDK gameplay watermark.

Historical tested artifact/source/log hashes, later observed binary hashes and
preservation evidence are recorded in
[staged-camera-relative-artifacts-2026-11-11.json](staged-camera-relative-artifacts-2026-11-11.json).
Any subsequently authorized deployment scope is the matching core, managed host and HL mod only, with
backups. The statically imported `dinput8.dll` proxy is already byte-identical and
does not need replacement. Existing settings/manifests/logs/backups stay intact.
Older verification/deployment records are preserved as historical evidence.

## Manual acceptance after authorized deployment

1. Manually launch; test W/A/S/D and diagonals while rotating the visible orbit
   through 0/90/180/270°. Check that forward follows the horizontal viewing
   direction without altering native magnitude/speed. Orbit off must be native.
2. Test stationary/moving orbit, walls, close/distant scripted shots, movies,
   F9/alt-tab, pause/resume, death/respawn and owner replacement. Record routed
   heading/refusal counters from the Camera tab.
3. Check native Ctrl traversal and its release grace, chains, rails, independent
   wall Space and HL ownership handoff. Keep HL defaults **69 eye-height / 24
   forward-offset HL units**, persisted tuning and manual activation/pinning.
4. Test M1 slash and held M2 spin/hover, simultaneous/physical-key holds, remaps,
   conflict refusal and release-to-rearm. Attack names remain layout inference
   until gameplay confirmation. Test normal FOV off/on and 30/60/90/120° with
   orbit off/on, native exclusions and lens restoration.
5. Compare Animation off/on in the same scene with F9 closed: record game FPS,
   active simulation Hz, simulation seconds/wall second and SDK sample/render
   ms/query counts. A 100-Hz render target must not accelerate 60-Hz simulation.
6. Verify exact 1080p/4K Reset, monitor-refresh selection, HUD/movie fit, display
   rollback, manager scrolling, always-on watermark and Lua/C# API compatibility.
