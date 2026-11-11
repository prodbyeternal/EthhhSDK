# HL native keyboard hook — SHA-256 gate fix — 2026-11-11

## Outcome

The installed HL WASD lease refusal is explained and fixed in staging. The live
mod activated, then immediately stopped with `native WASD ownership unavailable`
because `InstallMovementInputHook()` returned false before its fingerprint check
or install log. The cause was an empty SHA-256 result from `Sha256FileHex`:
`BCryptGetProperty(BCRYPT_OBJECT_LENGTH, ...)` used the same `DWORD` as both the
property buffer and its returned byte count, corrupting `cbHashObject`. The
subsequent `BCryptCreateHash` failed, so the supported-executable check compared
an empty string and silently refused.

The matching core, managed host and HL mod are **staged, not installed or live
accepted**. No game process was launched, terminated, closed or restarted. The
game was already manually closed before the staged checks. A fresh manual-closure
confirmation is required before installation.

## Reproduction

Live evidence at `06:46:50` (installed core `d8dd2de3…`, managed host `6044ff7a…`,
HL mod `073334fa…`):

```text
06:46:50.902 [INF] [halflife_movement] activated (movement=1 camera=1)
06:46:50.907 [WRN] [halflife_movement] native WASD ownership unavailable; stopped instead of mixing movement directions
06:46:50.907 [INF] [halflife_movement] deactivated
```

There was no `HL opt-in WASD filter installed` line and no
`HL keyboard poll fingerprint mismatch` line. The manual pin remained valid
(`player: manual selection 0512F9A8`, no `manual strat … disappeared` until
shutdown at `06:52:05`).

A 32-bit MSVC probe reproduced the exact BCrypt sequence against the supported
`I-Ninja.exe`:

```text
buggy: ''
fixed: '49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0'
```

The supported executable SHA-256 is
`49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0`.
The keyboard-poll bytes at `0x4427A0` were also verified against the expected
13-byte sequence, so the next gate is intact.

## Fix

- `src/core/util.cpp`: separate `objectLen`/`objectCb` from the SHA-256 properties;
  check `BCryptHashData` and read errors; destroy the hash only when created.
- `src/core/hooks.cpp`: log the missing tick/MinHook flags, path, computed SHA-256,
  and `MH_CreateHook`/`MH_EnableHook` status instead of silently returning false.
- `src/core/input.cpp`: log the specific lease refusal (filter latch, manual mode,
  validity, address/class/handle identity, hook) and the keyboard-owner/protection
  reason that latches the filter.
- `tests/Sha256Tests.cpp`: calls the production `Sha256FileHex` against `"abc"`
  and a missing file.
- `tests/run-tests.ps1`, `tests/CMakeLists.txt`: build and run the new test.

## Regression evidence

`run-tests.ps1 -Configuration Release` passed:

- HL math: 23,721 assertions
- HL segment collision: 4,685 assertions
- managed interop: 331 assertions per six runs; additive 45; camera-only 45
- player learning: 8; strat identity: 33; consent/facing: 669
- native movement input: 41,876
- **native SHA-256 file hashing: 4 assertions**
- EthhhPatch policy: 24,415; projection: 274; camera: 13,947
- evidence tests: 25; hot-path guards: 14
- x86 export ABI: 219 required exports / 217 P/Invoke bindings

The new test would have failed with the old BCrypt aliasing: it returned an empty
hash instead of the known SHA-256 of `"abc"`.

Build log:
`C:\Users\Administrator\AppData\Local\Temp\opencode\ethhh-build-sha-fix-20261111.log`,
SHA-256 `5ed3b7d6eb6066ce288469d7b44f4e9f54e1de148fd3037d7a2562a332992b11`.

Test log:
`C:\Users\Administrator\AppData\Local\Temp\opencode\ethhh-tests-sha-fix-20261111.log`,
SHA-256 `bc357fb2e652ba22f860e8c187144308aab1828efa9ebf77470f8d8e9163b96d`.

## Staged artifacts

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `dinput8.dll` | 90,624 | `b20f48212c610edacddd1b36c96e428f333741db28b10deb0d0df76b098cbf57` |
| `EthhhSDK.dll` | 1,258,496 | `b89a7734af4909c0fa544109330e97bfe4f6ca31030312125032b0d465603bf3` |
| `EthhhSDK\EthhhSDK.Managed.dll` | 58,880 | `03a9a2c8ffd9fd6c145e30435fad15d7fa4f2534ad9fc4c68c9a5854a64d6bc9` |
| `EthhhSDK\mods\HalfLifeMovement\Mod.dll` | 34,816 | `bc3c82f502bcde0830d6059b4470ba4f873eea034b6c4a1b14a5673e67b0d53a` |

The managed host and HL mod were rebuilt with the current toolchain and have
different binary hashes from the installed copies even though their source is
unchanged. Deployment should still use the matching tested set.

The live set remains the 06:42 install: core `d8dd2de3…`, host `6044ff7a…`, mod
`073334fa…`, proxy `b20f4821…`. The game is currently closed; the pre-install
baseline and process-count check must be captured again immediately before
installation.

## Manual acceptance still required

After installation and a manual launch, confirm:

1. Activating HL movement logs `HL opt-in WASD filter installed`.
2. W/S/A/D follow the rendered view after mouse turns without native sideways
   motion.
3. Ctrl release from chains/rails, jumps, landing, death and respawn behave.
4. F9/focus, animation, walls, slopes/platforms and performance are unchanged.

This report proves the SHA gate and offline policy only. It does not prove live
hook installation, keyboard filtering, collision, animation, cutscene
preservation or high-refresh smoothness. No game control was performed.
