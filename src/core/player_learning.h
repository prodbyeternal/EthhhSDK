#pragma once
#include <cmath>

namespace eth::game {
// Temporal evidence, not an identity proof. Followers can score just like the
// player; the tracker therefore ranks candidates and never auto-pins them.
struct MovementEvidence {
    float activeTravel = 0, idleTravel = 0;
    float activeTime = 0, idleTime = 0;
    void Sample(float distance, float dt, bool moving, bool suspended) {
        if (suspended || !std::isfinite(distance) || !std::isfinite(dt) || dt <= 0 || distance < 0) return;
        if (moving) { activeTravel += distance; activeTime += dt; }
        else { idleTravel += distance; idleTime += dt; }
    }
    float ActiveSpeed() const { return activeTime > 0 ? activeTravel / activeTime : 0; }
    float IdleSpeed() const { return idleTime > 0 ? idleTravel / idleTime : 0; }
    float Score() const {
        if (activeTime < 0.75f || idleTime < 0.75f) return 0;
        float difference = ActiveSpeed() - IdleSpeed() * 1.5f;
        return std::isfinite(difference) && difference > 0 ? difference : 0;
    }
};
}
