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
  hit count and collision object. The HL adapter uses support/body ray bundles;
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
   pushing W into wall contact, V climbs and normal contact limits descent.

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
4/34/68-HL body heights; live testing must tune these against the visible body.

`tests/run-tests.ps1` exercises math, the real managed host/mod against a fake
engine, learning scores, and native exports. `eth_chrome_preview` uses a hidden
window/off-screen D3D9 target and synthetic ImGui navigation to check card layout
and Start/Exit/close routing; it never calls the game entry gate or launches the
game. These tests cannot establish live collisions or character synchronization.
