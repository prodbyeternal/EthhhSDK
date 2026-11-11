# Render-only orbit camera verification — 2026-11-11 01:29

## Outcome

The Release x86 build passes the full offline regression and all 11 off-screen
UI profiles. It implements third-person render-rate orbit outside HL mode and
restores native camera/View state after each complete frame. **Live smoothness,
cutscene preservation and collision behavior remain unaccepted.**

No game process was launched, stopped, closed, restarted or given automated input
by the assistant. `install.ps1` was not run. During verification the user confirmed
copying the build immediately after it finished; native live files now match
staging, but the managed host and HL mod remain older.

## Verified implementation scope

- `View_tick` (`0x43EFA0`) snapshots previous/current positions at `+0x20/+0x40`
  and quaternions at `+0x30/+0x50`. The render builder (`0x43F290`) interpolates
  those and builds world/inverse/product. Updating Camera alone did not provide a
  coherent visible viewpoint and CPU-culling camera.
- Orbit now overrides both View pose endpoints and Camera world/inverse/product
  inside `App_render_step` (`0x43E170`). Its `__finally` restores Camera
  `[0,0x40)` / `[0x80,0x100)` and View `[0x20,0x120)` after the full render call.
- Capture refusal does not arm a restore. Replaced objects receive no stale
  writes. Intervening changes to the installed Camera/View pose are preserved;
  regions still carrying our pose are restored and future orbit is disabled.
- Target translation interpolates two completed simulation ticks with one-tick
  latency. Mouse orientation changes every render. Targets are sampled after
  the tracker, mods and all SDK tick callbacks, not before their position writes.
- Movies yield immediately with a 0.5-second hold extended by each movie Present.
  HL's explicit external-camera ownership has priority.
- Scripted shots use conservative distance/aim/roll/lens/cut checks. A suspected
  shot requires explicit F9 resume and 15 stable simulation ticks. Object
  replacement retains this latch and cancels any old-owner resume request.
- Five padded obstruction rays are capped to one batch per 60-Hz interval, with
  immediate retraction and eased extension. Query failure retains native camera.
- The two-byte render split remains reversible (`7C F2` -> `90 90` at `0x43D96C`);
  original timer debt, 60-Hz simulation and the four-tick cap remain unchanged.

## Safety audit fixes

1. Ninja-facing previously read `current.angles[3]` from a three-float snapshot
   and copied four angles. The shared native writer now changes only yaw at
   `+0x174` and quaternion `[+0x180,+0x190)`, never `+0x17C` padding or other angles.
   Nonfinite yaw produces no writes; finite yaw is wrapped before conversion.
2. Native exact-Ninja reacquisition now enforces prior explicit unique-Ninja pin
   consent before scanning. Consent cannot come from auto mode, learning, an
   attachment, an incomplete/ambiguous list or persisted activation settings.
   Player loss preserves consent; resets/mode changes/other selections revoke it.
3. Camera/player/View replacement no longer silently clears a suspected-script
   latch. Byte-restoration tests now also cover foreign View writes and replaced
   View objects, in addition to Camera replacement/ownership changes.

No existing ABI layouts or export names changed in this audit. Native facing is
still experimental: these changes do not establish safe model-node refresh or
authoritative traversal/grounded state.

## Offline results

| Check | Result |
| --- | --- |
| Release Win32 build | Pass |
| HL legacy math | 23,721 assertions |
| Real managed host/mod against fake engine | 99 assertions per run; three scale cases |
| Player learning | 8 assertions |
| Strat identity | 33 assertions |
| Native pin consent/facing byte ranges | 669 assertions |
| Display/cadence policy | 24,415 assertions |
| Projection/runtime policy | 274 assertions |
| Camera policy/quaternion/restoration | 13,947 assertions |
| Fingerprinted static evidence | 24 tests; 127 anchors / 39 slices / 34 edges |
| Hot-path/source safety guards | 10 tests |
| x86 ABI | 218 required exports / 216 P/Invoke bindings |
| Off-screen managers | All 11 profiles pass |
| `git diff --check` | Pass; CRLF conversion warnings only |

An initial log-capture wrapper reported a failure status while formatting
successful unittest stderr as PowerShell error records. The direct unwrapped
`tests/run-tests.ps1` rerun passed, including both unittest suites and ABI checks.
The intentional managed GUI exception is a tested cleanup fixture, not a game
crash. Offline policy/shared-writer tests do not execute the original engine.

## Artifacts and preservation

Staged sizes: core **1,250,304 bytes**, proxy **90,624 bytes**, managed host
**58,368 bytes**, HL mod **30,208 bytes**. All staged SHA-256 values are recorded in
[the dated artifact manifest](staged-camera-artifacts-2026-11-11-0129.json).

The preserved original baseline remains **16/16 Data files unchanged**, with the
game executable, `ininja-offsets.h`, both settings files, live managed host and
live HL assembly also unchanged. The complete earlier 24-file snapshot is
**22/24 unchanged**: only the live core/proxy differ, matching the user's confirmed
manual copy. Those copies were preserved. Saves and live manifests were not edited.
HL eye/forward defaults and persisted values remain **69 / 24 HL units**; the saved
640x480 fullscreen @100-Hz display plan was not replaced.

Evidence under `C:\Users\Administrator\AppData\Local\Temp\opencode`:

- `ethhh-camera-baseline-20261111.json` — original baseline, retained;
- `ethhh-camera-audit-preservation-20261111-0127.json` — 22/24 snapshot;
- `ethhhpatch-camera-audit-static-20261111-0127.json` — bounded disassembly;
- `ethhh-camera-audit-artifacts-20261111-0131.json` — staged/live/source hashes
  captured at 01:29, including read-only live-manifest hashes;
- `ethhh-camera-audit-build-20261111-0127.log` — Release build output;
- `ethhh-camera-audit-regression-20261111-0127.log` — initial capture-wrapper output.

## Pending manual acceptance

1. Confirm manual game closure; install a consistent native/managed/mod set with
   `install.ps1` **without `-Force`**, preserving settings/manifests. Restart manually.
   Files copied on disk do not update modules already loaded by a running process.
2. With HL off, test moving-target and stationary mouse orbit, nearby/distant
   scripted shots, movies, explicit resume, native-camera toggle, walls, death,
   respawn, F9, focus loss and external HL ownership handoff.
3. Measure actual viewpoint smoothness separately from Present count and body
   animation. Verify supported 1080p/4K/100-Hz modes, about 100 game Presents/s,
   60 active ticks/s and one simulation-second per real second.
4. Compare unpinned/pinned, F9 open/closed, HL on/off and patch on/off overhead;
   test HUD/movie fit, visibility, loads, restore and rollback.

A definitive native cutscene flag, authoritative traversal/death/grounding
contracts, calibrated collision hull and safe post-movement model refresh remain
unverified. Gameplay-shaped cinematic shots may evade heuristics; legitimate
camera changes may falsely suspend orbit. These limitations are not resolved by
the offline pass counts.
