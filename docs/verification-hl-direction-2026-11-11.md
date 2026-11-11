# HL direction and traversal handoff — 2026-11-11

## Outcome

The matching Release Win32 core, managed host and HL mod are **installed, not live
accepted**. The final offline regressions and all 11 off-screen manager profiles
pass. Sideways movement and jump sticking after Ctrl chain/rail release still
require manual acceptance in the original game.

After fresh confirmation that the user had manually closed I-Ninja, the matching
set was installed at **2026-11-11 06:42** without `-Force`; the game was not
launched, terminated or restarted by the assistant. Backup
`_backup/20261111-064203` preserves the exact pre-install live files. See
[the deployment record](deployment-hl-direction-2026-11-11-0642.md).

## Reproduction and changes

The installed prior `mods/HalfLifeMovement/Mod.dll`, SHA-256
`644c6a88ec453e705b2629beecab4fc44ae726ce4b9c3d5a6188cb6da0c0e434`,
was copied read-only into an isolated fake-engine sandbox. Both new tests failed
with exit code 1:

- `direction-only`: camera-relative W retained perpendicular native walking.
- `traversal-only`: continued walking perpetually refreshed traversal grace after
  Ctrl release, preventing HL ownership and jumping from returning.

These are adapter reproductions, not execution of the original engine's scripts.
The staged real host/mod now passes the corresponding regression cases.

### Native WASD separation

- `src/core/movement_input_policy.h` implements a lease for **one upcoming native
  tick**, consumed once and renewed only after a successful HL callback. A missing
  callback cannot leave movement filtering latched. Inactive ticks skip identity
  reads when no lease is pending.
- `src/core/hooks.cpp` lazily hooks keyboard poll `0x4427A0`. Installation requires
  the supported executable SHA-256 and its rebased 13-byte prologue. Static
  evidence establishes poll caller `0x441E66` through `0x43FEDF`, reached at
  `0x43E0E7` before script-object update `0x43E10F` inside the native tick.
- `src/core/input.cpp` freshly checks Manual tracking, exact Ninja role, address,
  class and full self handle at lease consumption. Filtering requires the current
  owner path `[0x50FFA4] -> +0x2C -> +8` and a writable, bounded 512-byte buffer.
  Invalid owner/buffer access refuses writes and prevents subsequent leases.
- Only scan codes `0x11/0x1E/0x1F/0x20` and their `+256` press-edge bytes are
  cleared. SDK raw key reads remain readable. Ctrl, jump, arrows, mouse, gamepad
  and every other byte remain untouched. F9/focus loss bypass or expire ownership;
  F2/F3 bypass consumption, while panic/disable/traversal revoke the lease.
- Additive and camera-only modes never request filtering. The adapter continues
  starting from the engine's current pose: `RemainingHorizontal` conservatively
  credits matching advance and preserves other native displacement. It does not
  subtract guessed collision, platform or script corrections.

### Bounded traversal handoff

`mods/HalfLifeMovement/src/HalfLifeMovementMod.cs` no longer interprets continued
translation as proof of native traversal ownership. After Ctrl release, grace
counts down regardless of walking. Two consecutive skin-close, non-rising
supported samples allow early handback; otherwise grace expires after 30 ticks.
Native airborne vertical authority survives timeout until queried landing, so
timeout alone does not authorize an HL impulse in midair. These are heuristics,
**not verified native chain/rail/death/grounded flags**.

The experimental additive API is available as:

- C#: `Input.LeaseMovementKeys(player, enabled)`;
- Lua: `input.lease_movement_keys(player, enabled)`;
- native: `int __cdecl EthInput_LeaseMovementKeys(unsigned int player, int enabled)`.

Pass false to revoke; renew only after a successful movement callback. Existing
ABI names/layouts remain unchanged: Strat 56, Camera 152, Identity 40,
TraceSegment 24 and TraceHit 36 bytes. The new mod requires the matching
core/managed host; **deploy core, host and mod together**.

## Offline verification

| Check | Result |
| --- | --- |
| Final Release Win32 build | Pass |
| Legacy HL math | 23,721 assertions |
| Synthetic segment collision | 4,685 assertions |
| Real managed host/mod against fake engine | 331 assertions each, six runs |
| Additive / camera-only ownership | 45 / 45 assertions |
| Native one-tick lease and eight-byte filter policy | 41,876 assertions |
| Player learning / strat identity / consent-facing | 8 / 33 / 669 assertions |
| Display cadence / projection / orbit restore | 24,415 / 274 / 13,947 assertions |
| Fingerprinted static evidence / source guards | 25 / 14 tests |
| Native x86 exports / P/Invoke resolution | 219 / 217 |
| Off-screen manager profiles | 11/11 |

New managed cases cover eight view yaws, each WASD direction, mouse turns, raw-key
readability, Ctrl-held yielding, continued walking after release, held Auto-hop,
fresh jumps, unsupported rail timeout and queried landing. Existing native
takeoff, near-floor, ceiling, wall, correction, identity and query-failure refusal
cases also pass. Fixture error messages and failure-path warnings in the log are
expected exercised cases; the suite finishes successfully.

The current-tuning test initially used a 55-tick flight wait, too short for gravity
531. Only its waits were extended to 90 ticks; no runtime physics or user tuning
was changed to make that test pass. Native tests exercise shared lease/byte-range
policy, not a live keyboard detour. Previews do not prove in-game rendering.

## Preservation, settings and staged artifacts

The 68-file baseline contains eight intentional existing-file edits; the other
**60/60 files remain byte-identical**, with zero unexpected changes. This includes
the 16 Data files, executable, user `../ininja-offsets.h`, saves, configuration,
live DLLs, existing manifests and historical reports. Additional native/managed
and Lua source snapshots were captured before edits. All 17 recorded current
source hashes and all five staged artifact hashes were rechecked at
`2026-11-11T03:02:44+01:00`; live DLL hashes still match the prior installed set.

Camera defaults remain **69 HL units eye height / 24 forward**. Preserved saved
settings are eye 101, forward 5, FOV 130, Auto-hop on, bunny cap off, scale 0.025,
jump X, gravity 531, air acceleration 40, jump speed 268.33 and threshold 134.
Saved `active=1` does not authorize reactivation. Direct inspection of the
unchanged `config/EthhhSDK.ini` gives a **1280×1024 fullscreen / 100-Hz plan**;
the earlier 640×480 summary is not the current saved file. Neither plan proves
the mode is applied or that viewpoints render smoothly at 100 Hz.

Artifact sizes, SHA-256 values, live comparison, deployment paths and hashed log
references are in [the additive manifest](staged-hl-direction-artifacts-2026-11-11-0300.json).
Core/host/mod differ from live; the proxy and HL manifest match and need no content
change. Existing settings and manifests must not be replaced during installation.

Evidence under `C:\Users\Administrator\AppData\Local\Temp\opencode`:

- `ethhh-hl-direction-verification-20261111-0300.json` — final preservation,
  staged/live/source hashes and log references;
- `ethhh-hl-direction-baseline-20261111-023336.json`;
- `ethhh-hl-direction-source-backup-20261111-023729` and
  `ethhh-hl-direction-lua-baseline-20261111-0254.json`;
- `ethhh-hl-direction-old-repro-20261111-0242\evidence.json`,
  `direction-only.log` and `traversal-only.log`;
- `ethhh-hl-direction-build-20261111-025454.log`;
- `ethhh-hl-direction-regression-20261111-025738.log`;
- `ethhh-hl-direction-preview-20261111-025900.log`;
- `ethhh-hl-direction-*-static-20261111-*.txt` — bounded keyboard layout,
  owner, caller and tick-order disassembly fragments, not complete functions.

## Deployment gate and manual acceptance

Fresh manual-closure confirmation was obtained, live DLLs/settings/protected files
were rebaselined immediately before installation, and the matching set was
deployed at 06:42 without `install.ps1 -Force`. All four expected live hashes and
all four pre-install backups verify; 63/63 non-deployed baseline files remain
unchanged, the live manifest is unchanged, and no game process was running after
installation. Restart remains manual. The installed pre-fix set is partly older:
the core already matched staging before this install, while the managed host and
HL mod were replaced. Details and hashes are in
[the deployment record](deployment-hl-direction-2026-11-11-0642.md).

Then manually test:

1. W/S/A/D alignment at several view angles, including mouse turns while moving.
2. Ctrl chain/rail release while continuing to walk and while holding X with
   Auto-hop on; fresh/tapped HL jumps and configured native jumping.
3. F9, focus loss/recovery, animation, diagonal walls/retreat, slopes/platforms,
   native traversal, death and respawn. Use F3 if behavior is wrong.
4. Separately: orbit/resume and scripted-camera/movie preservation, HL camera
   handoff, 1080p/4K/high-refresh display, and unpinned/pinned/F9/HL/patch timing.

Sparse segment bundles remain an approximation, not a calibrated native hull or
start-solid contract. Native grounded/death/out-of-bounds/traversal/cutscene state
and safe post-movement model refresh remain unverified. Post-tick position writes
can still conflict with native animation or scripted movement. Arrows/gamepad and
script/platform motion are not suppressed. This update establishes no live proof
of correct collision, preserved cutscenes, smooth 100-Hz viewpoints or eliminated
lag. GTA-style camera behavior and final Valve agreement/attribution packaging
remain pending.
