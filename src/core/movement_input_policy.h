#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace eth::input {

// One upcoming native tick only. A missing/failed mod callback cannot leave
// keyboard ownership latched. The runtime supplies a freshly checked identity.
class MovementKeysLease {
public:
    bool Request(uint32_t addr, uint32_t cls, uint32_t handle)
    {
        addr_ = addr; cls_ = cls; handle_ = handle;
        pending_ = addr && cls && handle;
        return pending_;
    }
    void Reset() { pending_ = false; }
    bool Pending() const { return pending_; }
    bool Consume(bool manual, bool valid, bool exactNinja, bool focused,
                 bool overlay, bool ctrl, float dt, uint32_t addr, uint32_t cls, uint32_t handle)
    {
        const bool requested = pending_;
        pending_ = false;
        return requested && manual && valid && exactNinja && focused && !overlay && !ctrl &&
               std::isfinite(dt) && dt > 0 && dt <= 0.1f &&
               addr == addr_ && cls == cls_ && handle == handle_;
    }
private:
    uint32_t addr_ = 0, cls_ = 0, handle_ = 0;
    bool pending_ = false;
};

// Verified keyboard poll 0x4427A0: 256 down bytes, then 256 press-edge bytes.
// Leave Ctrl, jump, arrows, mouse/gamepad and every other byte untouched.
inline bool FilterMovementKeys(uint8_t* keyboard, size_t size, bool owned)
{
    if (!owned) return true;
    if (!keyboard || size < 512) return false;
    constexpr size_t scan[] = { 0x11, 0x1e, 0x1f, 0x20 }; // DIK_W/A/S/D
    for (size_t key : scan) keyboard[key] = keyboard[256 + key] = 0;
    return true;
}

} // namespace eth::input
