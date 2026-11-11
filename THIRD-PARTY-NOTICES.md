# EthhhSDK third-party notices

Local experimental Windows x86 SDK. This notice is not a blanket license for
EthhhSDK, the game, or supplied assets, and is not distribution approval.

| Component | Attribution / terms | Source notice | Staged notice under `build/dist` |
| --- | --- | --- | --- |
| Dear ImGui 1.91.8 | Omar Cornut and contributors; MIT | `src/third_party/imgui/LICENSE.txt` | `licenses/Dear-ImGui.txt` |
| MinHook | Tsuda Kageyu and contributors; BSD 2-Clause and included third-party terms | `src/third_party/minhook/LICENSE.txt` | `licenses/MinHook.txt` |
| Lua 5.4 | Copyright (C) 1994-2024 Lua.org, PUC-Rio; MIT | `src/third_party/lua/LICENSE.txt` (notice from `lua.h`) | `licenses/Lua.txt` |
| Liquid Glass template / DX9 adaptation | Copyright (c) 2026 poncipp; upstream MIT | `src/third_party/liquid_glass/LICENSE` | `licenses/Liquid-Glass.txt` |
| Inter Medium and SemiBold | The Inter Project Authors (https://github.com/rsms/inter); SIL OFL 1.1 | `src/third_party/liquid_glass/THIRD-PARTY-NOTICES.md` | `licenses/Liquid-Glass-Upstream-Notices.md` |
| Font Awesome Free solid/brands fonts and icon mapping | Fonticons, Inc.; fonts SIL OFL 1.1, icons CC BY 4.0, code MIT; https://fontawesome.com | Same upstream font/icon notice | Same staged upstream notice |
| Half-Life movement reference | Half Life 1 SDK Copyright(c) Valve Corp.; Valve HLSDK agreement, not MIT | `mods/HalfLifeMovement/LICENSE` | `licenses/Valve-HLSDK.txt` and `mods/HalfLifeMovement/LICENSE` |

The complete upstream Liquid Glass notices are retained without edits. Their
relative paths and references to FreeType, Sacramento and UIcons describe the
original DX11 example, not this runtime. EthhhSDK builds with ImGui's default
font rasterizer and embeds Inter and Font Awesome; it does not link FreeType or
embed Sacramento/UIcons. Correct license locations for this port are above.
ImGui's bundled stb code/default-font notices remain in its vendored sources.

## Valve reference

Only `halflife-steam_legacy/pm_shared/pm_shared.c` was used as the HL movement
reference. No BunnymodXT reference is used. This is a C# reimplementation of
selected movement math, not the HL engine or a verified native hull-trace port.
The full agreement from `halflife-steam_legacy/LICENSE` accompanies the mod.
Its free-distribution and modified-Valve-game language needs review before any
public release of this non-Valve-game adaptation. Copying the text does not
resolve applicability or grant permission. Do not claim distribution clearance.

## Supplied audio and game assets

The five WAV files in `sounds to be used` were supplied by the user and retained
unchanged. Their embedded PCM16 adaptation does not establish ownership or
redistribution rights. No external audio license was supplied; review those
rights before distributing binaries that contain the sounds.

I-Ninja is (c) 2003 Namco Hometek Inc. / Argonaut Games plc. The SDK does not
redistribute the original executable, Data archives, movies or saves. It is a
fan project, not affiliated with those owners or with Valve.

Apple and Apple product names are trademarks of Apple Inc. The Liquid Glass
appearance is an independent implementation, not Apple code/assets or an
endorsed product. The original example disclaimer is retained at
`src/third_party/liquid_glass/DISCLAIMER.md` and staged as
`licenses/Liquid-Glass-Disclaimer.md`; this port uses DX9, not its DX11 demo.
