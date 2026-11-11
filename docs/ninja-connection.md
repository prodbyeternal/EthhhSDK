# Ninja identity and read-only ownership connection

Implemented after explicit permission to resume player work on 2026-11-10.
The user's `../ininja-offsets.h` is unchanged; the following supplements its
older inferred player claims for the fingerprinted executable only.

## Evidence and implementation

- Class name: `strat+0x1A8 -> class+0x04 -> NUL-terminated ASCII`. Maximum 63
  characters; unreadable, empty, non-ASCII and unterminated names are refused,
  never truncated into an apparent identity. Class/name pointers are rechecked.
- Self/generation handle: `strat+0x1A0`. Auto requires a complete bounded list
  with exactly one class named **Ninja** and no unreadable class names; no prefix/case/proximity fallback.
  Manual pins include class and self handle. Learning never auto-pins.
- Native `0x41C5D0` resolves handle slots at `0x51019C + (handle & 0x1FF)*8`,
  compares the **full** generation handle and obtains the slot pointer at `+4`.
  SDK resolution also checks slot stability, live-list membership and owner's
  self handle. An unresolved owner address is `0`, not a guessed nearby object.
- `+0x238` is reported as a raw field for all identities, but owner resolution is
  enabled only for corroborated `NinjaRageAttachment`, `NinjaSpeech`, and `Camera`.
  Do not assume `NinjaHead` or `NinjaSword` share that contract.
- Read-only addresses: model/resource `+0x268`, animation/control `+0x26C`,
  node-matrix storage `+0x13C`. They are diagnostic pointers, **not** CStrats,
  iterable arrays with verified counts, or approved engine-call arguments.

Archived class metadata, historical ownership, the 46-node asset hierarchy and
VM paths are retained in the temporary research directory
`%LOCALAPPDATA%/Temp/opencode/ininja-data-20261110/vm-findings.md` and its JSONs.
No captured address, class relocation base or historical handle is hard-coded.

## Additive API

Existing `EthStrat`/C# `StratInfo` remains **56 bytes**, and `CameraInfo` remains
**152 bytes**. Three new cdecl exports are added:

```c
// Class name: returns 1 on success. Buffer must fit the whole name plus NUL.
int EthGame_StratClassName(unsigned addr, char* buffer, int capacity);
// Identity: returns 1 on success, otherwise clears the entire output.
int EthGame_ReadStratIdentity(unsigned addr, EthStratIdentity* output);
// Informational text may truncate; it is never used for identity decisions.
int EthGame_PlayerDetectionInfo(char* buffer, int capacity);
```

`EthStratIdentity` is 40 bytes, sequential x86 layout:

| Offset | Type | Field |
| --- | --- | --- |
| 0, 4, 8 | `uint32` | address, class pointer, self handle |
| 12, 16 | `uint32` | raw owner handle, resolved owner address |
| 20, 24, 28 | `uint32` | model, animation, node-matrix addresses |
| 32 | `int32` | role: unknown=0, Ninja=1, attachment=2, related=3, other=4 |
| 36 | `int32` | owner-field-known (0 or 1) |

C#:

```csharp
string name = Game.GetStratClassName(Game.PlayerAddr);
if (Game.TryReadStratIdentity(Game.PlayerAddr, out StratIdentity identity))
    ModLog.Info(name + " self handle " + identity.SelfHandle.ToString("X8"));
```

Lua strat/candidate tables add `class_name` (nil if unreadable), `self_handle`
and `role`. `game.strat_identity(addr)` returns nil on failure, otherwise
`addr`, `class`, `self_handle`, `raw_owner_handle`, `owner_field_known`,
`owner_addr`, `model`, `animation`, `node_matrices`, and `role`.
`game.player_detection_info()` supplies human-readable status.

## Native control safety follow-up — 2026-11-11

`EthGame_PlayerReacquireExactNinja` now enforces prior selection in the native
tracker, not just in its managed caller. Consent is session-local and requires an
explicit selection of the unique exact Ninja from a complete bounded list, with
live membership and full self-handle validation. Reacquisition still requires a
complete list with a unique exact Ninja and manual tracking mode. Losing the
entity preserves consent for respawn; reset, mode changes, learning, and a
different/failed explicit selection revoke it. Persisted `active=1` cannot grant
this permission. Selecting through an explicit SDK API is selection intent; the
SDK does not distinguish a UI click from a caller requesting that selection.

The experimental `EthGame_SetNinjaFacing` path no longer copies a fourth float
from `StratView::angles[3]` (the snapshot has only three elements). It writes just
the yaw scalar at `strat+0x174` and the four-component quaternion at `+0x180`,
leaving pitch, roll, `+0x17C` padding and other neighbors untouched. Nonfinite yaw
is refused, and finite yaw is wrapped before script-unit/quaternion conversion.
Exact identity/full-handle checks remain. This does not establish atomic writes,
an authoritative post-movement transform refresh, or synchronized model nodes.

Offline tests exercise the shared native consent/facing policy and exact byte
write ranges. The managed fake engine checks the interop contract separately;
neither is a live respawn or model-facing acceptance result.

## What this deliberately does not implement

HL activation still requires a manual pin and now additionally requires exact
Ninja identity. It rechecks class and self handle every active movement tick and
stops on identity/read failure. It does **not** auto-activate after detection.

Verified original order is: manager scripts/animation, second-pass collision and
motion, transform/spatial finalization, remaining original `Fn_Tick`, **then SDK
callbacks and HL writes**. `0x413B30` is not a pure refresh: it runs scripts and
animation. Neither it nor `0x413040`/`0x41C5D0` is invoked by the new connection.
No script bypass, model-node writes, movement-hook relocation or collision
contract is introduced. The root/cache gap in Inspector is non-atomic evidence,
not proof that all matrices or script state are synchronized.

SafeRead now catches access faults after its protection check. That limits
allocation-lifetime crashes; it does not make snapshots atomic or pin objects.
Visible whole-body movement must still be checked manually before trusting HL.
