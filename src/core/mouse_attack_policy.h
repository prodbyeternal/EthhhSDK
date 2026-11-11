#pragma once
#include <cstddef>
#include <cstdint>
#include <array>

namespace eth::input {

// keyboard.cfm: action 9 = square/slash, action 7 = circle/spin.
// Attack pressure is read from the DOWN byte (0x442FE3), not a repeated tap.
struct AttackBinding {
    uint32_t type = 6, scan = 0, unused = 0;
    uint32_t gateType = 6, gateScan = 0, gateUnused = 0;
    uint8_t inverted = 0, padding[3]{}; // native loader writes ONE byte at +0x18
    bool Supported() const {
        // Never synthesize movement, jump, modifier, menu or SDK hotkeys.
        return type == 0 && scan > 1 && scan < 256 && unused == 0 &&
            gateType == 6 && gateScan == 0 && gateUnused == 0 && inverted == 0 &&
            scan != 0x11 && scan != 0x1e && scan != 0x1f && scan != 0x20 &&
            scan != 0x39 && scan != 0x1d && scan != 0x9d && scan != 0x2a &&
            scan != 0x36 && scan != 0x38 && scan != 0xb8 && scan != 0x1c &&
            scan != 0x9c && scan != 0x0f && !(scan >= 0x3b && scan <= 0x44) &&
            scan != 0x57 && scan != 0x58;
    }
};
static_assert(sizeof(AttackBinding) == 28, "native action binding stride");

constexpr size_t kSlashAction = 9, kSpinAction = 7;
using AttackBindingTable = std::array<AttackBinding, 20 * 2>;
inline bool AttackBindingsSupported(const AttackBindingTable& table) {
    const auto& slash = table[kSlashAction * 2];
    const auto& spin = table[kSpinAction * 2];
    if (!slash.Supported() || !spin.Supported() || slash.scan == spin.scan) return false;
    // Both descriptors of every OTHER action, including keyboard gates. An
    // unsafe custom remap leaves all original keyboard/gamepad controls native.
    for (size_t i = 0; i < table.size(); ++i) {
        const auto& binding = table[i];
        for (size_t action : {kSlashAction, kSpinAction}) {
            const uint32_t scan = table[action * 2].scan;
            if (i / 2 != action && ((binding.type == 0 && binding.scan == scan) ||
                (binding.gateType == 0 && binding.gateScan == scan))) return false;
        }
    }
    return true;
}

class MouseAttackPolicy {
public:
    void Reset() { armed_ = false; previous_[0] = previous_[1] = false; }
    bool Apply(uint8_t* keys, size_t bytes, const AttackBinding& slash,
        const AttackBinding& spin, bool eligible, bool left, bool right) {
        if (!keys || bytes < 512 || !eligible || !slash.Supported() ||
            !spin.Supported() || slash.scan == spin.scan) { Reset(); return false; }
        // Opening F9, losing focus or changing bindings requires release before
        // rearming. A held manager click must never become a gameplay attack.
        if (slash.scan != scan_[0] || spin.scan != scan_[1]) {
            Reset(); scan_[0] = slash.scan; scan_[1] = spin.scan;
        }
        if (!armed_) {
            if (left || right) return false;
            armed_ = true;
        }
        const bool mouse[2] = {left, right};
        for (int i = 0; i < 2; ++i) {
            const size_t scan = scan_[i];
            const bool held = keys[scan] != 0 || mouse[i];
            // Preserve every native edge. Only add a mouse edge when the merged
            // hold begins; joining a physical hold cannot retrigger an attack.
            if (mouse[i]) keys[scan] = 1;
            if (mouse[i] && !previous_[i]) keys[256 + scan] = 1;
            previous_[i] = held;
        }
        return true;
    }
private:
    bool armed_ = false, previous_[2]{};
    uint32_t scan_[2]{};
};

} // namespace eth::input
