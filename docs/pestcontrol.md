# PestControl

An optional **EthhhPatch > PestControl** toggle, off by default. The same toggle
in the pre-game manager saves the next session's setting (`pestcontrol=1` in
`[ethhhpatch]`). It does not enable HL, Auto+, interpolation, or a display patch.

- Searches every 15 native simulation ticks (four times per second at 60 Hz),
  not every rendered frame. Disabled tick path does no engine reads or locking.
- Matches only the exact, case-sensitive script class `Rat`.
- Requires a complete stable list, idle manager iterators, live generation handle,
  matching class, and bidirectional membership immediately before each request.
- Bounded to 8,192 inspected strats and 64 requests per search, with a 2 ms soft
  work budget and pass-local readable-region caching. Incomplete scans delete
  nothing. Further Rats are handled by a later search.
- Invokes the game's per-strat native 128 at `0x416F10` (cdecl self). This sets
  native pending flags `5`. The following StratMgr tick handles reference/handle
  invalidation, model/collision cleanup, script/lifetime teardown, and recycling.
  Native lifetime work may defer final recycling. SDK code never frees strats,
  unlinks lists, or fabricates death effects/rewards.
- Disabling stops future requests, not teardown already requested. It cannot
  resurrect removed Rats. A level reload or native respawn may create new Rats.
- Manager telemetry distinguishes requests, pending teardown, and observed
  generation-handle removals. A native-call/byte-guard failure blocks further
  requests for that run.

## Evidence and limits

The original executable is SHA-256
`49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0`.
The runtime verifies that file fingerprint, the complete native-128 body/table
entry, and teardown anchors before allowing requests. It rechecks the request
bytes/table before every search.

A read-only snapshot on 2026-11-11 at 22:25:28 found 14 exact `Rat` strats and
one `Ninja`, with a complete stable list at tick 75,471. Session-local pointers
were not persisted as identities. Snapshot evidence remains outside the repo in
`C:\Users\Administrator\AppData\Local\Temp\opencode\pestcontrol-identity-20261111-222528.json`.

**Do not use `0x424470`:** despite resembling a class-removal routine, it matches
`strat->class[0]`, a shared asset owner. It can delete non-Rat level objects.
`0x423C50` alone is only list unlinking, not destruction. Neither is called here.

Reproduce static disassembly with `python tools/inspect-pestcontrol.py`.
Offline policy and evidence tests do not delete any game entity. Live deletion,
level transitions, respawns, and gameplay consequences still require manual
acceptance; no live PestControl execution or installation has been performed.
