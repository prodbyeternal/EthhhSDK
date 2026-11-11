# Identifying the ninja and checking the HL adapter

## What is known

- The authoritative reference is `../ininja-offsets.h`.
- The live script-object list starts at VA `0x511A38` for the supported image.
- Strat position is four floats at `+0x160`; the adapter writes only XYZ,
  leaving the fourth component untouched. Class metadata is at `+0x1A8`.
- Exact class names are read through `strat+0x1A8 -> class+0x04`. Auto requires
  one exact `Ninja` in a complete bounded list, and never falls back to proximity.
- Self handle `+0x1A0` is checked to reject same-class address reuse.
- A verified attachment native resolves owner handle `+0x238` through manager
  slots, then derives attachment transforms from owner's model-node matrices.
  The SDK exposes this connection read-only; it does not invoke the native.
- The native collision manager's 0x44-byte segment-query pipeline is now bounded
  and exposed only for the pinned exact Ninja. It returns hit point, normal,
   hit count and collision object. The HL adapter uses support/body/vertical ray bundles;
  this is not yet a proven capsule/hull equivalent.
- Historical selections in the log are not verified identities or reusable
  addresses. Even matching class pointers do not prove an object is the ninja.
- `+0x1C4` is not a verified parent/world transform. The position getter at
  `0x416760` passes it to `0x410440`, whose inspected branches randomize
  coordinates. Do not dereference it as a model-transform pointer.

## Manual procedure

1. Restart I-Ninja manually after installing. Load a level and leave HL in
   standby (F3 stops it if needed).
2. Open F9 > Inspector > Player. Auto should report a unique exact `Ninja`.
   Inspect the **class name**, not an address copied from a previous session.
   If absent or ambiguous, do not choose an attachment or weaken guards.
3. Optional: click **Learn candidates**, close F9, remain idle for roughly 1 second, move the ninja with WASD or arrow
   keys for roughly 3 seconds, then stop for 2 seconds. Overlay-visible,
   unfocused, and nonpositive/invalid-time-step ticks do not count. Gamepad-only
   movement is not correlated by this learner.
4. Reopen Inspector > Player. Exact `Ninja` is prioritized; learning scores use
   `max(0, active_speed - 1.5 * idle_speed)`, requiring at least 0.75 seconds of
   both active and idle samples. Nothing is selected automatically; followers
   can tie. An all-zero ranking means retry rather than trust the first row.
5. **Pin exact Ninja**, then explicitly click the **+X nudge (0.25 game units)**
   with HL off. Close F9 and inspect the *visible ninja*, not just the viewpoint.
6. A saved return is available for up to 30 seconds while that same live,
   class-and-handle-matching object remains pinned and HL is off. It restores the saved
   coordinates, not the game's later movement; use promptly. Selection changes,
   unlinking or expiry discard it. No probe or return happens automatically.
7. If the whole visible ninja moved coherently, test F2 in an open area. Check walking, V jump,
   F3, overlay pause, focus loss, and disable/re-enable. The model must follow
   the simulated position. Press F3 if behavior is wrong.
8. Test slopes, platform edges and walls before traversal abilities. Ctrl yields
   position authority to native chain/rail/script motion without leaving HL mode.
   Large native displacements also enter a bounded traversal grace period.
9. After death, only a complete-list unique exact `Ninja` can be reacquired;
    ambiguous identities and attachments remain refused. While airborne and
    pushing W into wall contact, the configured HL jump key climbs and normal
    contact limits descent; native vertical ownership takes priority.

If a candidate's raw position changes but the visible ninja does not, that is
**not** evidence that the camera-only movement is fixed. It may be the wrong
object, a script-restored position, or a cached model transform. Report which
object moved (if any); the next investigation is model synchronization and tick
ordering, not larger teleport writes or guessed pointer dereferences.

## Evidence files

`logs/player-learning-YYYYMMDD-HHMMSS.csv` records address, class, position,
flags/type, active and idle speeds, score, and sample durations. Top candidates
and explicit probe results also appear in `logs/EthhhSDK.log`. A successful
write log only proves the memory write succeeded; it does not prove rendering
or player identity. The CSV speeds are game units per unpaused fixed-tick second.

## Calibration and automated coverage

HL velocity, gravity, jump carry and dimensions stay in HL units. Position and
camera writes multiply by `world_units_per_hl_unit`. Keep the estimate `0.025`
until the player is identified, then calibrate against observed walking distance
and character dimensions. Camera defaults are 69 HL units eye height and 24 HL
units forward offset. Collision sampling currently uses a 12-HL-unit radius and
4/36/72-HL body heights, a 72-HL head height and 0.5-HL skin; live testing must
tune these against the visible body. A two-HL-unit support probe finds nearby
surfaces, but only skin-close contact can grant jump permission.

## Jump/wall follow-up — 2026-11-11

The user confirmed that native jumping works with HL disabled but sticks with HL
active. Offline regression tests also reproduce unwanted Auto-hop airborne
re-triggering and native takeoff suppression in the installed older mod.

- HL flight uses explicit ownership, not positive height above takeoff. Holding
  Auto-hop cannot restart flight while ascending near a floor; it can hop again
  after a collision-confirmed landing. Descent can continue below takeoff.
- Native upward motion has priority and is never snapped back to the old floor.
  A native-flight latch survives a near-floor pause until skin-close landing.
- Horizontal sweeps start at the current native-corrected position, include a
  leading radius, and re-trace unconsumed sliding at walls/corners (three bumps
  maximum). "Replace game movement" now credits matching native advance rather
  than rewinding corrections/platform motion; it is not exact replacement.
- Five padded foot/head rays bound HL vertical/climb writes. Landing is based on
  actual queried support, not an old takeoff plane. Failed/malformed support,
  body, vertical or final-support queries refuse the entire position commit.
- Large native vertical corrections cancel the HL arc rather than resurrecting
  a stale absolute pose. F9 suspension clears motion history/ownership.

See [the dated verification report](verification-hl-jump-2026-11-11-0216.md).
These are segment approximations and post-tick writes, **not** verified native
ground/death/out-of-bounds state or a calibrated whole-body collision hull.
Unexpected respawn and model synchronization still require in-game acceptance;
the game's out-of-bounds protection was not disabled.

`tests/run-tests.ps1` exercises math, the real managed host/mod against a fake
engine, learning scores, and native exports. `eth_chrome_preview` uses a hidden
window/off-screen D3D9 target and synthetic ImGui navigation to check card layout
and Start/Exit/close routing; it never calls the game entry gate or launches the
game. These tests cannot establish live collisions or character synchronization.

## Direction/traversal follow-up — 2026-11-11

The installed 02:21 mod reproduces sideways camera-relative W movement with
injected perpendicular native keyboard walking. It also fails to return HL
ownership after Ctrl release when continued movement perpetually refreshes grace.

- Replacement now requests a **one-native-tick WASD lease**, filtering only W/A/S/D
  down/press-edge bytes after the exact native keyboard poll. SDK raw keys remain
  readable. Ctrl, native jump keys, arrows, gamepad and collision/platform/script
  displacement are not filtered. No native position correction is subtracted.
- The poll hook is lazy, supported-executable SHA-256/prologue gated, and only
  filters the current input owner's writable bounded keyboard buffer. Fresh manual
  exact-Ninja address/class/full-handle checks gate every consumed lease.
- Ctrl always bypasses filtering. F9/focus loss/panic/disable/failure stop or expire
  ownership; missing callbacks cannot retain a lease indefinitely. Camera-only
  and additive movement never request filtering.
- Traversal grace counts down regardless of translation after Ctrl release.
  Two skin-close non-rising supported samples can hand back early; otherwise the
  30-tick grace expires. Native airborne authority survives timeout until landing.
  These are heuristics, **not verified native rail/chain state flags**.

Experimental API (renew only after a successful tick; pass false to revoke):
`Input.LeaseMovementKeys(player, enabled)` in C#,
`input.lease_movement_keys(player, enabled)` in Lua, and
`int __cdecl EthInput_LeaseMovementKeys(unsigned int player, int enabled)`.
Existing ABI layouts/names remain unchanged; the new export requires installing
the matching core/managed host/mod together. Existing settings and 69/24 HL-unit
camera defaults are preserved, including different user-persisted offsets.

Offline coverage includes eight camera yaws, mouse turns, each WASD direction,
continued walking/held Auto-hop after Ctrl, unsupported rail timeout and landing,
plus byte-range/identity/lease refusal tests. **Live movement, native animation,
traversal, collision and camera acceptance are still required.** See
[the dated report](verification-hl-direction-2026-11-11.md). The matching set was
installed on 2026-11-11 at 06:42 after fresh manual-closure confirmation; backup
`_backup/20261111-064203` preserves the pre-install files. See
[the deployment record](deployment-hl-direction-2026-11-11-0642.md).
