#pragma once
namespace eth::game {
// Existing C/Lua/managed values are stable; Auto+ is appended, never renumbered.
enum class PlayerMode { Auto=0, Manual=1, Learning=2, AutoPlus=3 };
constexpr bool PinnedMode(PlayerMode mode) { return mode==PlayerMode::Manual || mode==PlayerMode::AutoPlus; }
constexpr bool ValidPlayerMode(int mode) { return mode>=0 && mode<=3; }
}
