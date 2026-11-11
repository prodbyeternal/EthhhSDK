# EthhhSDK — I-Ninja (Windows x86)

Local experimental SDK: a `dinput8.dll` entry-gate proxy, native core, pre-game
ImGui manager, F9 in-game manager, Lua 5.4 trainers, and a .NET Framework C# host.

**Supported executable:** SHA-256
`49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0`.
The offset reference is the parent folder's `ininja-offsets.h`; inferred fields
and player identity are not compatibility guarantees.

## Build and install

Requirements: Visual Studio 2022 with the x86 C++ toolset and Roslyn, CMake on
PATH, Python 3 (Capstone for static research tests), and .NET Framework 4.x. Dependencies are vendored in
`src/third_party`.

```powershell
.\build.ps1 -Reconfigure
.\tests\run-tests.ps1
.\tests\run-preview-tests.ps1
# Close I-Ninja manually first:
.\install.ps1
```

Artifacts stage under `build/dist`. Installation backs up replaced DLLs under
`_backup/<timestamp>` and preserves existing configuration and mod manifests.
Do not use `install.ps1 -Force`: it terminates the game. Restart manually after
installation. The fake test `EthhhSDK.dll` under `build/tests/interop` must never
be installed into the game directory.

## Controls and mods

- F9: manager; pause-on-overlay and input handling are configurable.
- `mods/trainer/mod.lua`: Lua trainer and inspection example.
- `mods/HalfLifeMovement/src`: C# movement/camera showcase. F2 activates, F3
  stops, WASD moves, V jumps. Loading or re-enabling it always starts in standby.
- Lua exposes `game`, `input`, `ui`, mod settings and lifecycle hooks. Native
  bindings are in `src/core/lua_bindings.cpp` and `src/core/lua_imgui.cpp`.
- Managed APIs are in `managed/src`; native C ABI definitions are in
  `src/core/c_api.cpp` and `src/core/imgui_c.cpp`. C# assemblies remain loaded
  until restart. Lua hooks are SDK events, not arbitrary native detours.

## Half-Life adapter: important limits

Exact class-name detection now reads `strat+0x1A8 -> class+0x04`; Auto accepts
only a unique `Ninja`, not camera proximity. Pinning is still required for HL;
attachments, camera/speech and unknown identities are rejected. The verified
owner-handle path is exposed read-only in the Inspector, Lua and C#;
see [the connection API and safety contract](docs/ninja-connection.md).

**Visible-model synchronization and the safe movement contract remain unverified.**
The adapter writes the selected strat's
raw XYZ at `+0x160` after the original game tick. It does not instantiate a
GoldSrc player hull and has no verified I-Ninja hull traces or grounded flag.
Bounded segment sweeps do not establish native whole-body collision or internal
fall state. Test in open areas and use F3 if motion or respawn is wrong.

Learn now ranks input-versus-idle motion and never auto-pins a winner. Camera
helpers and followers can resemble the player. See
[the manual verification procedure](docs/hl-verification.md).

`world_units_per_hl_unit` converts HL dimensions at the game boundary. Its
default `0.025` is an **estimate, not a measured world scale**. At this scale:

| HL quantity | I-Ninja quantity |
| --- | --- |
| 320 units/s walking speed | 8 game units/s |
| 69-unit eye height | 1.725 game units |
| 24-unit forward offset | 0.6 game units |
| ~45-unit jump apex | ~1.125 game units |

Jump uses explicit flight ownership and half-gravity motion with floor/ceiling
sweeps; holding Auto-hop cannot rebase an ascending near-floor arc. Native
takeoffs have priority. Horizontal sweeps preserve native corrections and
re-trace wall slides rather than rewinding last tick's position. See
[the jump/wall follow-up](docs/verification-hl-jump-2026-11-11-0216.md).
Offline tests do not prove live collisions, out-of-bounds state, camera
handedness, world units, or character rendering.

The FPS-camera defaults are **69 HL units eye height** and **24 HL units forward
offset**, intended to keep the Ninja body behind the first-person view. Whole-body
synchronization remains unverified; existing user settings remain authoritative.

Manual player pinning uses cached validity plus lightweight handle/list checks;
it no longer rebuilds the complete strat candidate list every tick. Writable
player/camera data is copied directly under SEH rather than changing page
protection and flushing the instruction cache for every movement write.

## EthhhPatch status

The manager has an **EthhhPatch** page with 1080p/4K/monitor-size planning,
windowed/fullscreen targets, measured Present/tick cadence, projection diagnostics,
and an acknowledged in-game experimental Apply/Restore transaction. Saving a
plan does **not** apply it; the pre-game launcher cannot change the display.

The experimental runtime uses the game's mode/Reset lifecycle, verifies the
resulting parameters, and rolls back mismatches. It enables real zero-tick render
frames while retaining the original fixed-60 timer/debt/tick cap, repairs the
verified aspect-blind culler, contains checked 4:3 HUD projections, advances the
render-driven HUD pulse from completed ticks, uniformly fits 640x480 movies, and
paces Presents to the target Hz. This staged build has not yet been installed or
live-accepted; unsupported modes are refused rather than guessed.
See [EthhhPatch development status](docs/ethhhpatch.md).

Outside HL mode, the optional third-person mouse orbit updates the actual View
pose every render and restores native camera/View bytes after the frame. It
yields to movies, HL ownership and suspected scripted shots. After a suspected
shot, use F9 **Resume orbit after scripted camera**; cutscene detection is still
conservative, not a verified native cutscene flag. Unchecking orbit retains the
native camera without restoring/reapplying the display patch.

## Attribution / distribution status

- Dear ImGui 1.91.8: Omar Cornut and contributors, MIT.
- MinHook 1.3.4: Tsuda Kageyu, BSD 2-Clause.
- Lua 5.4.7: Lua.org / PUC-Rio, MIT.
- Movement reference: Valve's `halflife-steam_legacy/pm_shared/pm_shared.c` in
  the parent folder. BunnymodXT is not used.

Vendored dependency license files remain with their sources. Final Valve
agreement text and consolidated attribution are pending; this is not a
distribution-ready package. No affiliation with Namco or Argonaut Games.
