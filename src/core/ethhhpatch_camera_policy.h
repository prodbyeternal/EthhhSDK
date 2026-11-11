#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace eth::patch::runtime {

struct CameraPoint {
    float x = 0, y = 0, z = 0;
    CameraPoint operator+(CameraPoint p) const { return {x + p.x, y + p.y, z + p.z}; }
    CameraPoint operator-(CameraPoint p) const { return {x - p.x, y - p.y, z - p.z}; }
    CameraPoint operator*(float s) const { return {x * s, y * s, z * s}; }
    float Dot(CameraPoint p) const { return x * p.x + y * p.y + z * p.z; }
    float Length() const { return std::sqrt(Dot(*this)); }
    bool Finite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
};

enum class OrbitState {
    Waiting, Following, Scripted, Movie, External, Unavailable, Paused, CollisionFailed
};

inline const char* OrbitStateName(OrbitState state)
{
    switch (state) {
    case OrbitState::Following: return "mouse orbit following";
    case OrbitState::Scripted: return "scripted camera hold (auto-resume)";
    case OrbitState::Movie: return "yielding to movie";
    case OrbitState::External: return "HL / external camera owns control";
    case OrbitState::Unavailable: return "waiting for exact Ninja / camera";
    case OrbitState::Paused: return "paused / unfocused";
    case OrbitState::CollisionFailed: return "collision query unavailable; native camera retained";
    default: return "waiting for a playable camera";
    }
}

// Free-look third-person orbit. Earlier builds demanded a narrow native distance,
// aim alignment, low roll and fifteen stable ticks, then latched permanently after
// one ambiguous sample; ordinary gameplay cameras could never qualify and the
// mouse appeared dead. This policy keeps only broad sanity checks, acquires in a
// few simulation ticks and treats suspicious shots as a short hold that recovers
// automatically. A real camera cut or a sustained independent camera pan refreshes
// the hold so cutscenes are not dragged around by the mouse.
class OrbitPolicy {
public:
    OrbitState state = OrbitState::Waiting;
    CameraPoint previous{}, current{};
    float yaw = 0, pitch = 0, distance = 6.0f, collisionDistance = 6.0f;
    float minDistance = 2.5f, maxDistance = 15.0f;
    float preferredDistance = 0; // wheel zoom survives reacquisition; 0 = native
    float nativeDistance = 0;
    bool initialized = false, scriptedHold = false;

    static constexpr float kMinPitch = -1.25f;
    static constexpr float kMaxPitch = 1.05f;

    void Reset() { *this = OrbitPolicy{}; }
    void OwnerChanged() {
        const bool hold = scriptedHold;
        const double until = holdUntil_;
        const float zoom = preferredDistance;
        Reset();
        preferredDistance = zoom;
        scriptedHold = hold; holdUntil_ = until;
    }

    void RequestResume()
    {
        resumeRequested_ = true;
        scriptedHold = false;
        holdUntil_ = 0;
        stableTicks_ = 0;
        if (state == OrbitState::Scripted) state = OrbitState::Waiting;
    }

    void Suspend(OrbitState reason)
    {
        state = reason;
        initialized = false;
        stableTicks_ = 0;
        haveSample_ = false;
        if (reason == OrbitState::Scripted) scriptedHold = true;
        else {
            scriptedHold = false;
            holdUntil_ = 0;
        }
    }

    bool Sample(uint32_t tick, double now, CameraPoint target, const float world[16], float fov,
        bool discontinuity)
    {
        if (!std::isfinite(now) || !target.Finite() || !world || discontinuity) {
            Suspend(OrbitState::Unavailable);
            return false;
        }
        if (haveSample_ && tick == tick_) return state == OrbitState::Following;

        for (int i = 0; i < 16; ++i) if (!std::isfinite(world[i])) {
            Suspend(OrbitState::Unavailable); return false;
        }
        if (std::abs(world[3]) > 0.001f || std::abs(world[7]) > 0.001f ||
            std::abs(world[11]) > 0.001f || std::abs(world[15] - 1) > 0.001f) {
            Suspend(OrbitState::Unavailable); return false;
        }
        const CameraPoint right{world[0], world[1], world[2]}, up{world[4], world[5], world[6]};
        if (std::abs(right.Length()-1)>0.05f || std::abs(up.Length()-1)>0.05f ||
            std::abs(right.Dot(up))>0.05f) {
            Suspend(OrbitState::Unavailable); return false;
        }

        const CameraPoint camera{world[12], world[13], world[14]};
        const CameraPoint forward{world[8], world[9], world[10]};
        const CameraPoint delta = target - camera;
        const float length = delta.Length();
        const float forwardLength = forward.Length();
        if (!camera.Finite() || !forward.Finite() || !std::isfinite(length) || length < 0.001f ||
            !std::isfinite(fov)) {
            Suspend(OrbitState::Unavailable);
            return false;
        }
        nativeDistance = length;
        const float aim = delta.Dot(forward) / (std::max)(0.001f, length * forwardLength);
        if (std::abs(forwardLength-1)>0.05f || std::abs(right.Dot(forward))>0.05f ||
            std::abs(up.Dot(forward))>0.05f) { Suspend(OrbitState::Unavailable); return false; }

        // Broad plausibility only. A real gameplay camera may sit far away or
        // look past the ninja; those shots must still be drivable.
        bool scripted = length > 45.0f || length < 0.35f ||
            forwardLength < 0.75f || forwardLength > 1.25f ||
            aim < 0.15f ||
            std::abs(world[1]) > 0.45f || fov < 0.15f || fov > 3.2f;

        bool cut = false;
        if (haveSample_ && tick - tick_ <= 4) {
            const CameraPoint relative = camera - target;
            const CameraPoint camMove = camera - lastCamera_;
            const CameraPoint targetMove = target - current;
            const CameraPoint independent = camMove - targetMove;
            const double dt = (std::max)(1u, tick-tick_) / 60.0;
            if ((relative - lastRelative_).Length() > 6.0f ||
                forward.Dot(lastForward_) < 0.20f || std::abs(fov - lastFov_) > 0.12f) {
                cut = true;
            } else if (aim < 0.80f && independent.Length() / dt > 1.5f) {
                if (++motionTicks_ > 12) motionTicks_ = 12;
            } else if (motionTicks_ > 0) {
                motionTicks_ -= 2;
            }
        }
        if (cut) motionTicks_ = 0;

        previous = haveSample_ && tick - tick_ <= 4 && (target - current).Length() < 8.0f
            ? current : target;
        current = target;
        tick_ = tick;
        sampleTime_ = now;
        haveSample_ = true;
        lastRelative_ = camera - target;
        lastForward_ = forward;
        lastCamera_ = camera;
        lastFov_ = fov;

        // An ordinary native orbit keeps aiming at the player and is eligible.
        // Off-target sustained motion is only a heuristic for scripted pans;
        // there is still no verified in-engine cutscene flag.
        if (motionTicks_ >= 8) scripted = true;
        if (cut) scripted = true;
        if (scripted) {
            scriptedHold = true;
            holdUntil_ = (std::max)(holdUntil_, now + kHoldSeconds);
            initialized = false;
            stableTicks_ = 0;
            state = OrbitState::Scripted;
            return false;
        }
        if (scriptedHold) {
            if (now < holdUntil_) {
                initialized = false;
                stableTicks_ = 0;
                state = OrbitState::Scripted;
                return false;
            }
            scriptedHold = false;
            holdUntil_ = 0;
        }

        if (state == OrbitState::Following && initialized) {
            resumeRequested_ = false;
            return true;
        }
        if (++stableTicks_ < 3) {
            state = OrbitState::Waiting;
            return false;
        }
        if (!initialized) {
            yaw = std::atan2(delta.x, delta.z);
            pitch = std::clamp(std::asin(std::clamp(delta.y / length, -1.0f, 1.0f)),
                kMinPitch, kMaxPitch);
            const float wanted = preferredDistance > 0 ? preferredDistance
                : std::clamp(length, minDistance, maxDistance);
            distance = std::clamp(wanted, minDistance, maxDistance);
            collisionDistance = distance;
            previous = current; // do not blend across a cut, movie or ownership handoff
            initialized = true;
        }
        resumeRequested_ = false;
        stableTicks_ = 0;
        state = OrbitState::Following;
        return true;
    }

    CameraPoint Target(double now) const
    {
        const float alpha = std::isfinite(now)
            ? std::clamp(float((now - sampleTime_) * 60.0), 0.0f, 1.0f) : 1.0f;
        return previous + (current - previous) * alpha;
    }
    CameraPoint Forward() const
    {
        const float cp = std::cos(pitch);
        return {std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
    }
    void Mouse(float dx, float dy, float radiansPerCount)
    {
        if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(radiansPerCount)) return;
        constexpr float turn = 6.283185307179586f;
        const float mx = dx*radiansPerCount, my = dy*radiansPerCount;
        if (!std::isfinite(mx) || !std::isfinite(my)) return;
        yaw = std::remainder(yaw + mx, turn);
        pitch = std::clamp(pitch - my, kMinPitch, kMaxPitch);
    }
    void SetDistance(float value)
    {
        if (!std::isfinite(value)) return;
        distance = std::clamp(value, minDistance, maxDistance);
        preferredDistance = distance;
        if (collisionDistance > distance) collisionDistance = distance;
    }
    void Zoom(float notches)
    {
        if (!std::isfinite(notches)) return;
        SetDistance(distance - notches * 0.8f);
    }
    void Obstruction(float allowedDistance, double dt)
    {
        if (!std::isfinite(allowedDistance) || !std::isfinite(dt)) return;
        allowedDistance = std::clamp(allowedDistance, 0.0f, distance);
        if (allowedDistance <= collisionDistance) collisionDistance = allowedDistance;
        else {
            const float blend = 1.0f - std::exp(-float(std::clamp(dt, 0.0, 0.1)) * 8.0f);
            collisionDistance += (allowedDistance - collisionDistance) * blend;
        }
    }

private:
    static constexpr double kHoldSeconds = 1.5;
    bool haveSample_ = false, resumeRequested_ = false;
    int stableTicks_ = 0, motionTicks_ = 0;
    uint32_t tick_ = 0;
    double sampleTime_ = 0, holdUntil_ = 0;
    CameraPoint lastRelative_{}, lastForward_{}, lastCamera_{};
    float lastFov_ = 0;
};

// The verified row-vector quaternion->matrix builder (0x4B2680) uses the
// standard D3D signs. View_tick's matrix->quaternion returns the equivalent -q.
inline void OrbitQuaternion(float yaw, float pitch, float out[4])
{
    const float sy = std::sin(yaw * 0.5f), cy = std::cos(yaw * 0.5f);
    const float sp = std::sin(pitch * 0.5f), cp = std::cos(pitch * 0.5f);
    out[0] = -sp * cy; out[1] = cp * sy; out[2] = sp * sy; out[3] = cp * cy;
}

// Shared by the actual detour and fake-memory behavioral tests. Save exactly
// the regions changed by camera inverse/product refresh and View rendering.
// FOV, scripts, the object header, projection and player transforms are untouched.
struct RenderCameraBackup {
    uintptr_t camera = 0, view = 0;
    float product[16]{}, matrices[32]{}, viewState[64]{};
    float installedWorld[16]{}, installedPose[16]{};
    bool pending = false, installed = false;

    void MarkInstalled(const float world[16], const float pose[16])
    {
        std::memcpy(installedWorld, world, sizeof(installedWorld));
        std::memcpy(installedPose, pose, sizeof(installedPose));
        installed = true;
    }

    template<class Read> bool Capture(uintptr_t cameraAddress, uintptr_t viewAddress, Read read)
    {
        if (pending || !cameraAddress || !viewAddress) return false;
        if (!read(cameraAddress, product, sizeof(product)) ||
            !read(cameraAddress + 0x80, matrices, sizeof(matrices)) ||
            !read(viewAddress + 0x20, viewState, sizeof(viewState))) return false;
        camera = cameraAddress; view = viewAddress; pending = true; installed = false;
        return true;
    }
    template<class Read, class Write> bool Restore(uintptr_t liveCamera, uintptr_t liveView, Read read, Write write)
    {
        if (!pending) return true;
        pending = false;
        if (liveCamera != camera || liveView != view) return false; // never write stale/replaced objects
        float world[16]{}, pose[16]{};
        const bool cameraOurs = !installed || (read(camera + 0x80, world, sizeof(world)) &&
            std::memcmp(world, installedWorld, sizeof(world)) == 0);
        const bool viewOurs = !installed || (read(view + 0x20, pose, sizeof(pose)) &&
            std::memcmp(pose, installedPose, sizeof(pose)) == 0);
        // Preserve intervening script/mod writes; restore only regions still
        // carrying our own override. A changed owner disables future orbit.
        const bool a = cameraOurs && write(camera, product, sizeof(product));
        const bool b = cameraOurs && write(camera + 0x80, matrices, sizeof(matrices));
        const bool c = viewOurs && write(view + 0x20, viewState, sizeof(viewState));
        return a && b && c;
    }
};

} // namespace eth::patch::runtime
