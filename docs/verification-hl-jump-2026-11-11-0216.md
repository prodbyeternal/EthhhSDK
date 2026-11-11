# HL jump/wall safety follow-up — 2026-11-11 02:16

## Outcome and user evidence

The updated Release Win32 build is staged, not installed. The full offline suite
and all 11 off-screen manager profiles pass. **Live wall escape, held-key jump,
slopes and unexpected out-of-bounds respawn remain unaccepted.**

The user reported wall sticking, jumps causing unexpected respawn, and both
native/HL jumping sticking until key release. The user confirmed native jumping
works normally after F3 disables HL. The installed older mod also fails the new
native-takeoff fake-engine test. An earlier Auto-hop test against that mod failed
the bounded-height check: a near-floor ascending arc could re-trigger/rebase.

No game process was launched, terminated, restarted, given input or modified in
memory by this continuation. No installer was run. The previous authorized
installation remains `_backup/20261111-013702`; a fresh manual-closure confirmation
is required before this staged update is installed.

## Source changes

`mods/HalfLifeMovement/src/HalfLifeMovementMod.cs`:

- Explicit HL/native flight latches replace the `jumpCarry > 0` ownership test.
  Auto-hop is eligible again only after genuine supported landing, never on an
  ascending near-floor frame. The configured jump key remains unchanged.
- Native takeoffs win immediately, including the first 0.4-HL-unit upward frame
  used in regression. No ordinary ground snap writes a native jump back down.
  Native flight survives a 1.5-HL-unit near-floor pause and key release.
- Nearby support alone is insufficient: only 0.5-HL-unit skin-close contact grants
  friction/jump permission. Fast native descent cannot re-ground/rejump above it.
- Horizontal replacement no longer starts at last tick's obsolete XYZ. It starts
  at the engine's current corrected pose and credits already-made advance along
  the desired HL direction. Opposing corrections/platform motion are preserved.
  This conservative policy is not exact native-walk replacement.
- HL half-gravity generates per-tick motion, with collision-driven landing even
  below takeoff. Bounded native gravity is compensated using the previous written
  Y, but corrections over 16 HL units/tick cancel the arc rather than restoring an
  obsolete pose. This threshold is a safety heuristic, not a native state flag.
- Wall climbing/descent limiting uses the same vertical sweep; native vertical
  authority takes priority. F9 suspension discards movement/flight ownership.

`mods/HalfLifeMovement/src/HlCollision.cs`:

- Reusable five-ray support, nine-ray body and five-ray vertical batches avoid
  the old per-tick array allocation. No batch exceeds the native 16-ray limit.
- Body sweeps include the leading radius and skin. Unconsumed motion/velocity is
  clipped against contacts and re-swept up to three times; untraced remaining
  motion is discarded at the budget limit.
- Foot/head sweeps include skin at their leading end too. A regression caught a
  sub-skin descent that otherwise left the root slightly below the lower floor
  under the user's 598-gravity tuning; it is now stopped at the queried floor.
- Hit points/normals must be finite and lie on their submitted ray; normals must
  have a plausible length before normalization. Failed/malformed queries refuse
  the final position write rather than treating failure as no obstruction.

`HlMove.cs` adds unclamped runtime half-gravity integration; the old takeoff-plane
clamp is retained only for standalone legacy math comparison. `managed/src/Native.cs`
adds read-only `TraceSegment.StartY/EndX/EndY/EndZ` convenience getters. All ABI
layouts, native exports and P/Invoke names remain unchanged. **Install the matching
managed host and HL mod together**; the new mod requires those managed getters.

## Offline checks

| Check | Result |
| --- | --- |
| Final Release Win32 build | Pass |
| Legacy HL math | 23,721 assertions |
| Shared segment helper, five scales + randomized corners | 4,685 assertions |
| Real host/mod against fake engine | 153 assertions per three scale runs |
| Auto-hop enabled | 152 assertions |
| Saved movement tuning: X / gravity 598 / airaccel 29.7 / jump 268.33 / threshold 20 | 152 assertions |
| Player learning / identity / native consent-facing | 8 / 33 / 669 assertions |
| Display cadence / projection / orbit | 24,415 / 274 / 13,947 assertions |
| Fingerprinted evidence / source guards | 24 / 12 tests |
| Native x86 exports / P/Invoke resolution | 218 / 216 |
| Off-screen manager profiles | 11/11 |

Managed cases include held takeoffs despite bounded native downward corrections,
native jump priority, near-floor pause/release, short falls, activation near
support, ceilings, lower-platform landing, wall slide/retreat, native push-out,
large correction yielding and refusal at each query stage before commit.
They do not execute the original engine's collision response, animation or scripts.

## Preservation and artifacts

All **29/29 baseline files remain byte-identical**, including 16 Data files,
`I-Ninja.exe`, `ininja-offsets.h`, Configuration, both SDK settings files, the live
DLL set and HL manifest. Settings/saves/manifests were not edited; eye/forward
defaults remain **69 / 24 HL units**. The user's saved X key and movement/display
choices were not replaced with test defaults.

Staged core/proxy/host/mod SHA-256 values are in
[the additive staged manifest](staged-hl-jump-artifacts-2026-11-11-0216.json).
The core was relinked by the full build without a new native movement-source edit;
the proxy still matches live, while core/host/mod differ from the installed set.

Evidence in `C:\Users\Administrator\AppData\Local\Temp\opencode`:

- `ethhh-hl-jump-baseline-20261111-015650.json`;
- `ethhh-hl-jump-old-mod-repro-20261111-021250.json` — older live mod read-only,
  copied only to the fake-engine sandbox, then replaced there with staging;
- `ethhh-hl-jump-build-20261111-021316.log`;
- `ethhh-hl-jump-regression-20261111-021349.log`;
- `ethhh-hl-jump-preview-20261111-021437.log`;
- `ethhh-hl-jump-verification-20261111-0216.json` — preservation/staged/live/source
  hashes, with `assistantInstalled=false` and `gameControlled=false`.

## Pending live acceptance / limitations

After manual closure and authorized installation, restart manually. Compare held
and tapped X (HL) against the configured native jump key, with Auto-hop on/off;
test low ceilings, diagonal wall approach/retreat, lower platforms, short falls,
wall climb, Ctrl chain/rails, F9, focus and respawn. Press F3 if behavior is wrong.

The engine's native grounded/death/out-of-bounds flags, collision hull and safe
post-movement model-node refresh are still unverified. Sparse rays can miss small
or irregular geometry and cannot report start-solid penetration; no guessed
depenetration teleport or fall/death suppression was added. Conservative body
dimensions can block valid motion. All writes still happen after the native tick,
so matching internal script/animation/fall state is not established by this pass.
Engine performance and unexpected respawn must be measured live, not inferred
from bounded query counts or offline assertions.
