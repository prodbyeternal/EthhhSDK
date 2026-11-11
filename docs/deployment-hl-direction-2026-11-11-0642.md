# HL direction/traversal deployment — 2026-11-11 06:42

## Authorization and pre-install state

The user confirmed they had manually closed I-Ninja and authorized installation of
the matching core, managed host and HL mod. The assistant did not launch,
terminate, close or restart the game. The first process check caught the game still
exiting; a recheck immediately before the baseline showed zero `I-Ninja` processes.

Pre-install baseline
`ethhh-hl-direction-install-baseline-20261111-064151.json` captured 67 files.
**The live core already matched the staged core `d8dd2de3…`; the managed host
`9b84d1ba…` and HL mod `644c6a88…` were older.** The assistant did not establish
how the core became current before this cycle. This installation produced the
first fully matching core/host/mod set and backed up the exact files present.

## Installation

`install.ps1` was run without `-Force` at 06:42:02 after the process check. It
reported:

```text
installed dinput8.dll
installed EthhhSDK.dll
installed EthhhSDK.Managed.dll
installed mods\HalfLifeMovement\Mod.dll
backup: C:\Users\Administrator\Desktop\ininja\EthhhSDK\_backup\20261111-064203
```

Install log: `ethhh-hl-direction-install-20261111-064202.log`, SHA-256
`17f4e46c7be52d5ca83ea3a74d9b8ea32df2d6c73d373b3e9802c803d9c6e478`.

Final installed set:

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `dinput8.dll` | 90,624 | `b20f48212c610edacddd1b36c96e428f333741db28b10deb0d0df76b098cbf57` |
| `EthhhSDK.dll` | 1,256,960 | `d8dd2de319e71bf5c900fbddb131ea9ba687f6ee5f473f17d64c95dbace53ab3` |
| `EthhhSDK\EthhhSDK.Managed.dll` | 58,880 | `6044ff7a81f0bcdcfbd4fe35f396e538421780f4ced7265977e070755b72ade8` |
| `EthhhSDK\mods\HalfLifeMovement\Mod.dll` | 34,816 | `073334faa78b45d26eaef7e1d5a8a2db862ac2d29d6fd23be19efecc3cd957d3` |

The proxy and HL manifest were already current. The live `mod.json` remains
`8c6487de965d1a50fcdc05dcdae421312eaf4edacf801a1e249e1428ee930d7c`;
the installer did not replace it.

Backup `_backup/20261111-064203` matches the exact pre-install live files:

| Backup entry | SHA-256 |
| --- | --- |
| `dinput8.dll` | `b20f48212c610edacddd1b36c96e428f333741db28b10deb0d0df76b098cbf57` |
| `EthhhSDK.dll` | `d8dd2de319e71bf5c900fbddb131ea9ba687f6ee5f473f17d64c95dbace53ab3` |
| `EthhhSDK.Managed.dll` | `9b84d1bac6152e4e894bcbebef25f114e8c8b230b26391585869f2747df0bfbe` |
| `mods\HalfLifeMovement\Mod.dll` | `644c6a88ec453e705b2629beecab4fc44ae726ce4b9c3d5a6188cb6da0c0e434` |

Verification
`ethhh-hl-direction-install-verification-20261111-064203.json` confirms
**4/4 expected live hashes, 4/4 backups, 63/63 non-deployed baseline files
unchanged, zero unexpected changes, unchanged manifest and zero game processes**
after installation. Settings, saves, protected Data/executable files, user
`ininja-offsets.h` and existing mod manifests were not replaced.

Installed artifact hashes and preservation evidence are also in
[the deployed manifest](deployed-hl-direction-artifacts-2026-11-11-0642.json).
The earlier [staged report](verification-hl-direction-2026-11-11.md) remains as the
pre-install offline evidence record.

## Manual acceptance still required

Launch I-Ninja manually and compare the installed behavior. Restarting is manual;
the assistant will not control the process.

1. Walk W/S/A/D at several view angles and after mouse turns; confirm movement
   follows the rendered view rather than native sideways motion.
2. Release Ctrl from chains/rails while continuing to walk and while holding X
   with Auto-hop on; confirm HL control and fresh jumps return without a mode
   toggle and without stuck takeoffs.
3. Test configured native jumping, Ctrl release in midair, landing, F9/focus loss,
   animation, diagonal walls/retreat, slopes/platforms, death and respawn. Use F3
   if behavior is wrong.
4. Separately test orbit/resume, cutscenes/movies, HL camera handoff, 1080p/4K/
   100-Hz display, and unpinned/pinned/F9/HL/patch performance.

The keyboard filter affects only native W/A/S/D bytes while HL owns movement; it
does not suppress arrows, gamepad, native jump or script/platform corrections.
Collision remains a segment-bundle approximation. Native grounded/death/
out-of-bounds/traversal/cutscene state and safe post-movement model refresh remain
unverified. No live smoothness, collision, cutscene or lag acceptance is claimed.
