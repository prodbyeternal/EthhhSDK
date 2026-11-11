#pragma once
#include <cmath>
#include <cstdint>

namespace eth::game {

// Session-local unique exact-Ninja pin consent, not an address or persisted setting.
// Losing a pin preserves consent; selecting another role or changing modes does not.
class NinjaPinConsent {
public:
    void Revoke() { authorized_ = false; }
    void Selected(bool uniqueExactNinja) { authorized_ = uniqueExactNinja; }
    bool AllowsReacquire(bool pinnedMode) const { return authorized_ && pinnedMode; }
private:
    bool authorized_ = false;
};

// Shared with offline byte-range tests. Only yaw (angles[1]) is written; the
// three-angle snapshot has no angles[3], and native +0x17C padding is not ours.
template<class Write> bool WriteNinjaFacingPose(uintptr_t angles, uintptr_t quaternion,
    float yawRadians, Write write)
{
    if (!angles || !quaternion || !std::isfinite(yawRadians)) return false;
    constexpr double turn = 6.28318530717958647692;
    const double yaw = std::remainder(double(yawRadians), turn);
    const float units = float(yaw * (4096.0 / turn));
    const float quat[4] = {0, float(std::sin(yaw * 0.5)), 0, float(std::cos(yaw * 0.5))};
    return write(angles + sizeof(float), &units, sizeof(units)) &&
        write(quaternion, quat, sizeof(quat));
}

} // namespace eth::game
