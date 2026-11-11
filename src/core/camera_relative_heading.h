#pragma once
#include "strat_identity.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace eth::patch::runtime::movement {

struct Owner {
    uintptr_t addr = 0, pClass = 0;
    uint32_t handle = 0;
    bool Valid() const { return addr && pClass && handle; }
};

struct VisibleHeading {
    Owner player, cameraOwner;
    uintptr_t camera = 0, view = 0;
    uint32_t tick = 0;
    double seconds = 0;
    float yaw = 0; // radians of the last successfully installed primary orbit
    bool Valid() const {
        return player.Valid() && cameraOwner.Valid() && camera && view &&
            std::isfinite(seconds) && seconds > 0 && std::isfinite(yaw);
    }
};

struct Gates {
    bool active = false, following = false, focused = false, overlay = false;
    bool externalCamera = false, leasedKeys = false, movie = false, restoreFailed = false;
    bool control = false;
    float timeStep = 0;
};

// No input edits or player writes. Eligibility is a single native-tick lease,
// based on a camera that really rendered, not a speculative orbit target.
class RoutingPolicy {
public:
    void Reset() { *this = RoutingPolicy{}; }
    void Invalidate() { visible_ = {}; eligible_ = false; }
    void Rendered(const VisibleHeading& visible) { visible_ = visible.Valid() ? visible : VisibleHeading{}; }
    void Begin(uint32_t tick, double seconds, const Gates& gates) {
        eligible_ = false;
        if (gates.control) traversalTicks_ = 30;
        else if (traversalTicks_) --traversalTicks_;
        if (!gates.active || !gates.following || !gates.focused || gates.overlay ||
            gates.externalCamera || gates.leasedKeys || gates.movie || gates.restoreFailed ||
            !std::isfinite(gates.timeStep) || gates.timeStep <= 0) {
            Invalidate(); return;
        }
        const double age = seconds - visible_.seconds;
        eligible_ = !gates.control && !traversalTicks_ && visible_.Valid() &&
            tick - visible_.tick <= 4 && std::isfinite(seconds) && age >= 0 && age <= 0.25;
    }
    void End() { eligible_ = false; }
    bool Eligible() const { return eligible_; }
    unsigned TraversalTicks() const { return traversalTicks_; }
    const VisibleHeading& Visible() const { return visible_; }
private:
    VisibleHeading visible_;
    bool eligible_ = false;
    unsigned traversalTicks_ = 0;
};

inline bool Compensate(float nativeHeading, float visibleYaw, float ownerYawUnits, float& output)
{
    if (!std::isfinite(nativeHeading) || !std::isfinite(visibleYaw) || !std::isfinite(ownerYawUnits) ||
        std::abs(nativeHeading) > 8192 || std::abs(ownerYawUnits) > 1048576) return false;
    constexpr double turn = 6.28318530717958647692;
    // ASL subsequently ADDS Camera strat+0x174. Cancel that native yaw, then
    // supply the displayed +Y / +Z-forward yaw; magnitude/dead-zone stay native.
    const double displayedUnits = std::remainder(double(visibleYaw), turn) * (4096.0 / turn);
    output = float(std::remainder(double(nativeHeading) + displayedUnits - ownerYawUnits, 4096.0));
    return std::isfinite(output);
}

// ESI points immediately AFTER the verified inline field read at entry to the
// thiscall getter. Package offsets are deliberately not used as live addresses.
inline bool HeadingSequence(const uint32_t (&words)[5])
{
    constexpr uint32_t expected[5] = {
        0x4275106A, 0x42761091, 0x4276500E, 0x0B747576, 0x4374701F};
    return std::memcmp(words, expected, sizeof(expected)) == 0;
}

struct Call {
    uintptr_t self = 0, other = 0, output = 0, caller = 0, nextInstruction = 0;
    uint32_t field = 0;
};

struct Context {
    bool enabled = false;
    VisibleHeading visible;
    uintptr_t manager = 0, ownerGlobal = 0, cameraGlobal = 0, viewGlobal = 0, expectedCaller = 0;
};

template<class Read> bool LiveOwner(const Owner& owner, uintptr_t manager, Read read)
{
    if (!owner.Valid() || owner.addr > UINT32_MAX - 0x2C0) return false;
    uint32_t pClass = 0, handle = 0;
    return read(owner.addr + 0x1A8, &pClass, 4) && pClass == owner.pClass &&
        read(owner.addr + 0x1A0, &handle, 4) && handle == owner.handle &&
        game::ResolveOwnerHandle(manager, handle, read) == owner.addr;
}

template<class Read> bool LiveContext(const Context& context, Read read)
{
    const auto& visible = context.visible;
    uint32_t owner = 0, camera = 0, view = 0, target = 0;
    return LiveOwner(visible.player, context.manager, read) && LiveOwner(visible.cameraOwner, context.manager, read) &&
        read(context.ownerGlobal, &owner, 4) && owner == visible.cameraOwner.addr &&
        read(context.cameraGlobal, &camera, 4) && camera == visible.camera &&
        read(context.viewGlobal, &view, 4) && view == visible.view &&
        read(visible.cameraOwner.addr + 0x238, &target, 4) && target == visible.player.handle;
}

template<class Read, class Write> bool Route(const Context& context, const Call& call, Read read, Write write)
{
    if (!context.enabled || !context.visible.Valid() || !context.expectedCaller ||
        call.caller != context.expectedCaller || call.field != 0x6A ||
        call.self != context.visible.player.addr || call.other != call.self || !call.output ||
        call.nextInstruction < 4 || call.nextInstruction > UINT32_MAX - 16 || call.nextInstruction % 4) return false;
    uint32_t registers = 0, words[5]{};
    if (!LiveContext(context, read) || !read(call.self + 0x1B4, &registers, 4) || !registers ||
        registers > UINT32_MAX - 0x1D8 || call.output != uintptr_t(registers) + 0x75 * 4 ||
        !read(call.nextInstruction - 4, words, sizeof(words)) || !HeadingSequence(words)) return false;
    float heading = 0, ownerYaw = 0, adjusted = 0;
    if (!read(call.output, &heading, 4) || !read(context.visible.cameraOwner.addr + 0x174, &ownerYaw, 4) ||
        !Compensate(heading, context.visible.yaw, ownerYaw, adjusted)) return false;
    uint32_t checkRegisters = 0, checkWords[5]{};
    float checkHeading = 0, checkYaw = 0;
    if (!LiveContext(context, read) || !read(call.self + 0x1B4, &checkRegisters, 4) || checkRegisters != registers ||
        !read(call.nextInstruction - 4, checkWords, sizeof(checkWords)) || !HeadingSequence(checkWords) ||
        !read(call.output, &checkHeading, 4) || std::memcmp(&heading, &checkHeading, 4) ||
        !read(context.visible.cameraOwner.addr + 0x174, &checkYaw, 4) || std::memcmp(&ownerYaw, &checkYaw, 4)) return false;
    return write(call.output, &adjusted, sizeof(adjusted)); // exactly one VM scratch float
}

} // namespace eth::patch::runtime::movement
