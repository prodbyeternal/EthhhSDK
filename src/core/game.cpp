#include "game.h"
#include "util.h"
#include "log.h"
#include "overlay.h"
#include "input.h"
#include <cmath>

namespace eth::game {

namespace {

constexpr float kTickDt = 1.0f / 60.0f;

IDirect3DDevice9* AsDevice()
{
    void* p = nullptr;
    if (!SafeRead(A(off::pD3DDevice), p)) return nullptr;
    return (IDirect3DDevice9*)p;
}

} // namespace

bool IsReady()
{
    return ValidateGameImage() && IsReadable((const void*)A(off::TickCount), 4);
}

uint32_t TickCount()
{
    uint32_t v = 0;
    SafeRead(A(off::TickCount), v);
    return v;
}

uint32_t FrameCount()
{
    uint32_t v = 0;
    SafeRead(A(off::FrameCount), v);
    return v;
}

float GameTime()
{
    float v = 0.0f;
    SafeRead(A(off::GameTimeF), v);
    return v;
}

float TimeStep()
{
    float v = 0.0f;
    SafeRead(A(off::TimeStepF), v);
    return v;
}

float TimeScale()
{
    float v = 1.0f;
    SafeRead(A(off::StratTimeScale), v);
    return v;
}

void SetTimeScale(float v)
{
    if (!std::isfinite(v)) return;
    if (v < 0.0f) v = 0.0f;
    if (v > 20.0f) v = 20.0f;
    if (overlay::DeferTimeScale(v)) return;
    SafeWrite(A(off::StratTimeScale), v);
}

uint16_t DebugFlags()
{
    uint16_t v = 0;
    SafeRead(A(off::DebugFlags), v);
    return v;
}

void SetDebugFlags(uint16_t flags)
{
    SafeWrite(A(off::DebugFlags), flags);
}

void ToggleDebugFlag(uint16_t bit, bool on)
{
    uint16_t f = DebugFlags();
    if (on) f |= bit; else f &= (uint16_t)~bit;
    SetDebugFlags(f);
}

const char* DebugFlagName(uint16_t bit)
{
    switch (bit) {
    case 0x001: return "LEVEL_SELECT";
    case 0x002: return "DEMO_LEVEL_SELECT";
    case 0x004: return "SOUND";
    case 0x008: return "PAL";
    case 0x010: return "DEBUG_CHEATS";
    case 0x020: return "DEBUG_TEXT";
    case 0x040: return "DEBUG_FLY";
    case 0x080: return "DEMO_BUILD";
    case 0x100: return "DEMO_MAGAZINE";
    default:    return "?";
    }
}

void* Device()   { return AsDevice(); }
void* Renderer() { void* p = nullptr; SafeRead(A(off::pRenderer), p); return p; }

void* GameHwnd()
{
    HWND h = nullptr;
    if (!SafeRead(A(off::hWnd), h) || !h || !IsWindow(h)) return nullptr;
    return h;
}

// ---------------------------------------------------------------------------
//  Camera
// ---------------------------------------------------------------------------
bool ReadCamera(CameraView& out)
{
    out = {};
    void* cam = nullptr;
    if (!SafeRead(A(off::pCamera), cam) || !cam) return false;
    if (!IsReadable(cam, 0x120)) return false;

    out.addr = (uintptr_t)cam;
    if (!SafeReadBytes((uintptr_t)cam + off::CamMat, out.world, sizeof(out.world))) return false;
    SafeReadBytes((uintptr_t)cam + off::CamViewMat, out.view, sizeof(out.view));
    out.x = out.world[12];
    out.y = out.world[13];
    out.z = out.world[14];
    SafeRead((uintptr_t)cam + off::CamFov, out.fov);
    SafeRead((uintptr_t)cam + off::CamParam104, out.param104);
    return true;
}

void UpdateView()
{
    void* cam = nullptr;
    if (!SafeRead(A(off::pCamera), cam) || !cam) return;
    if (!IsReadable(cam, 0x120)) return;
    using Fn = void(__thiscall*)(void*);
    Fn fn = (Fn)A(off::Fn_UpdateView);
    __try { fn(cam); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ETH_ERROR("Fn_UpdateView crashed"); }
}

bool SetCameraPosition(float x, float y, float z)
{
    void* cam = nullptr;
    if (!SafeRead(A(off::pCamera), cam) || !cam) return false;
    if (!IsReadable(cam, 0x120)) return false;
    float m[16];
    if (!SafeReadBytes((uintptr_t)cam + off::CamMat, m, sizeof(m))) return false;
    m[12] = x; m[13] = y; m[14] = z;
    if (!SafeWriteBytes((uintptr_t)cam + off::CamMat, m, sizeof(m))) return false;
    UpdateView();
    return true;
}

bool SetCameraWorld(const float m[16])
{
    void* cam = nullptr;
    if (!SafeRead(A(off::pCamera), cam) || !cam) return false;
    if (!IsReadable(cam, 0x120)) return false;
    if (!SafeWriteBytes((uintptr_t)cam + off::CamMat, m, sizeof(float) * 16)) return false;
    UpdateView();
    return true;
}

bool SetCameraFovRadians(float fov)
{
    void* cam = nullptr;
    if (!SafeRead(A(off::pCamera), cam) || !cam) return false;
    if (!SafeWrite((uintptr_t)cam + off::CamFov, fov)) return false;

    // Keep the View object coherent for the current frame (Fn_ViewTick will
    // recompute it next tick anyway).
    void* view = nullptr;
    if (SafeRead(A(off::pView), view) && view && IsReadable(view, 0x40)) {
        SafeWrite((uintptr_t)view + 0x08, fov);
        SafeWrite((uintptr_t)view + 0x0C, fov);
        float tanHalf = tanf(fov * 0.5f);
        SafeWrite((uintptr_t)view + 0x10, tanHalf);
        float secHalf = 1.0f / std::max(0.0001f, cosf(fov * 0.5f));
        SafeWrite((uintptr_t)view + 0x14, secHalf);
    }
    return true;
}

bool SetCameraFovUnits(float units)   { return SetCameraFovRadians(units * (2.0f * 3.14159265358979f / 4096.0f)); }
bool SetCameraFovDegrees(float deg)   { return SetCameraFovRadians(deg * (3.14159265358979f / 180.0f)); }
float CameraFovUnits(float radians)   { return radians * (4096.0f / (2.0f * 3.14159265358979f)); }
float CameraFovRadians(float units)   { return units * (2.0f * 3.14159265358979f / 4096.0f); }

void GetCameraBasis(float* right, float* up, float* forward)
{
    CameraView cam;
    if (!ReadCamera(cam)) {
        if (right) *right = 0;
        return;
    }
    if (right)   { right[0] = cam.world[0]; right[1] = cam.world[1]; right[2] = cam.world[2]; }
    if (up)      { up[0] = cam.world[4]; up[1] = cam.world[5]; up[2] = cam.world[6]; }
    if (forward) { forward[0] = cam.world[8]; forward[1] = cam.world[9]; forward[2] = cam.world[10]; }
}

bool MakeCameraWorld(float px, float py, float pz, float yawRad, float pitchRad, float rollRad, float out[16])
{
    float cy = cosf(yawRad), sy = sinf(yawRad);
    float cp = cosf(pitchRad), sp = sinf(pitchRad);

    // D3D left-handed basis: +Z is forward at yaw 0, +Y up, +X right.
    float fx = sy * cp;
    float fy = sp;
    float fz = cy * cp;

    float rx = 1, ry = 0, rz = 0; // world up x forward
    // right = normalize(cross(upWorld, forward))
    rx = 1.0f * fz - 0.0f * fy;
    ry = 0.0f * fx - 0.0f * fz;
    rz = 0.0f * fy - 1.0f * fx;
    float rl = sqrtf(rx * rx + ry * ry + rz * rz);
    if (rl < 1e-6f) { rx = 1; ry = 0; rz = 0; rl = 1; }
    rx /= rl; ry /= rl; rz /= rl;

    // up = cross(forward, right)
    float ux = fy * rz - fz * ry;
    float uy = fz * rx - fx * rz;
    float uz = fx * ry - fy * rx;

    if (rollRad != 0.0f) {
        float cr = cosf(rollRad), sr = sinf(rollRad);
        float nrx = rx * cr + ux * sr;
        float nry = ry * cr + uy * sr;
        float nrz = rz * cr + uz * sr;
        float nux = ux * cr - rx * sr;
        float nuy = uy * cr - ry * sr;
        float nuz = uz * cr - rz * sr;
        rx = nrx; ry = nry; rz = nrz;
        ux = nux; uy = nuy; uz = nuz;
    }

    out[0] = rx; out[1] = ry; out[2] = rz;  out[3] = 0.0f;
    out[4] = ux; out[5] = uy; out[6] = uz;  out[7] = 0.0f;
    out[8] = fx; out[9] = fy; out[10] = fz; out[11] = 0.0f;
    out[12] = px; out[13] = py; out[14] = pz; out[15] = 1.0f;
    return true;
}

// ---------------------------------------------------------------------------
//  Strats
// ---------------------------------------------------------------------------
bool ReadStrat(uintptr_t addr, StratView& out)
{
    out = {};
    if (!addr || !IsReadable((const void*)addr, 0x1C8)) return false;

    out.addr = addr;
    if (!SafeReadBytes(addr + off::StratPos, out.pos, sizeof(out.pos)) ||
        !SafeReadBytes(addr + off::StratAngles, out.angles, sizeof(out.angles)) ||
        !SafeReadBytes(addr + off::StratQuat, out.quat, sizeof(out.quat)) ||
        !SafeRead(addr + off::StratFlags, out.flags) ||
        !SafeRead(addr + off::StratTypeMask, out.typeMask) ||
        !SafeRead(addr + off::StratNext, out.next) ||
        !SafeRead(addr + off::StratPrev, out.prev) ||
        !SafeRead(addr + off::StratClass, out.pClass) ||
        !SafeRead(addr + off::StratSelfHandle, out.selfHandle)) return false;

    ReadClassName((uintptr_t)out.pClass, out.className, SafeReadBytes);
    uintptr_t checkClass = 0;
    uint32_t checkHandle = 0;
    if (!SafeRead(addr + off::StratClass, checkClass) || checkClass != (uintptr_t)out.pClass ||
        !SafeRead(addr + off::StratSelfHandle, checkHandle) || checkHandle != out.selfHandle) return false;

    // A strat with a non-finite position is not usable (mid-construction, etc.)
    if (!std::isfinite(out.pos[0]) || !std::isfinite(out.pos[1]) || !std::isfinite(out.pos[2]))
        return false;
    return true;
}

bool WriteStratPos(uintptr_t addr, float x, float y, float z)
{
    StratView strat;
    if (!addr || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !ReadStrat(addr, strat)) return false;
    const float position[3] = { x, y, z };
    return SafeWriteBytes(addr + off::StratPos, position, sizeof(position));
}

namespace {
bool LiveAddressMatches(uintptr_t wanted, void* expectedClass, uint32_t expectedHandle)
{
    if (!wanted || !expectedClass || !expectedHandle) return false;
    uintptr_t head = 0;
    if (!SafeRead(A(off::StratListHead), head)) return false;
    uintptr_t current = head;
    for (size_t guard = 0; current && guard < 8192; ++guard) {
        if (current == wanted) {
            uintptr_t pClass = 0;
            uint32_t handle = 0;
            uintptr_t checkHead = 0;
            return SafeRead(current + off::StratClass, pClass) && pClass == (uintptr_t)expectedClass &&
                SafeRead(current + off::StratSelfHandle, handle) && handle == expectedHandle &&
                ResolveOwnerHandle(A(off::StratMgrObject), handle, SafeReadBytes) == current &&
                SafeRead(A(off::StratListHead), checkHead) && checkHead == head;
        }
        uintptr_t next = 0;
        if (!SafeRead(current + off::StratNext, next) || next == current) return false;
        current = next;
    }
    return false;
}
}

bool ReadStratIdentity(uintptr_t addr, StratIdentity& out)
{
    out = {};
    StratView found;
    if (!ReadStrat(addr, found) || !found.className[0] || !found.selfHandle ||
        !LiveAddressMatches(addr, found.pClass, found.selfHandle)) return false;
    StratIdentity result;
    result.addr = (uint32_t)addr;
    result.pClass = (uint32_t)(uintptr_t)found.pClass;
    result.selfHandle = found.selfHandle;
    result.role = ClassRole(found.className);
    result.ownerFieldKnown = HasKnownOwnerField(found.className) ? 1 : 0;
    if (!SafeRead(addr + off::StratOwnerHandle, result.rawOwnerHandle) ||
        !SafeRead(addr + off::StratModel, result.model) ||
        !SafeRead(addr + off::StratAnimation, result.animation) ||
        !SafeRead(addr + off::StratNodeMatrices, result.nodeMatrices)) return false;
    if (result.ownerFieldKnown) {
        const uintptr_t owner = ResolveOwnerHandle(A(off::StratMgrObject), result.rawOwnerHandle, SafeReadBytes);
        StratView currentOwner;
        if (owner && ReadStrat(owner, currentOwner) && currentOwner.selfHandle == result.rawOwnerHandle &&
            LiveAddressMatches(owner, currentOwner.pClass, currentOwner.selfHandle))
            result.ownerAddr = (uint32_t)owner;
    }
    StratView check;
    uint32_t checkOwner = 0;
    if (!ReadStrat(addr, check) || check.pClass != found.pClass || check.selfHandle != found.selfHandle ||
        strcmp(check.className, found.className) || !SafeRead(addr + off::StratOwnerHandle, checkOwner) ||
        checkOwner != result.rawOwnerHandle ||
        ResolveOwnerHandle(A(off::StratMgrObject), found.selfHandle, SafeReadBytes) != addr) return false;
    out = result;
    return true;
}

bool TraceWorldSegments(uintptr_t exactNinja, const TraceSegment* segments, int count, TraceHit* hits)
{
    if (!segments || !hits || count <= 0 || count > 16) return false;
    memset(hits, 0, sizeof(TraceHit) * count);
    StratIdentity identity;
    if (!ReadStratIdentity(exactNinja, identity) || identity.role != StratRole::Ninja) return false;
    uint8_t filterA = 0, filterB = 0, checkA = 0, checkB = 0;
    if (!SafeRead(exactNinja + 0x2F6, filterA) || !SafeRead(exactNinja + 0x2F2, filterB)) return false;
    const uintptr_t manager = A(0x50EDD8);
    uint16_t queued = 0;
    if (!SafeRead(manager + 0x10, queued) || queued != 0) return false;
    using SubmitFn = uint32_t(__thiscall*)(void*, const void*);
    using ProcessFn = void(__thiscall*)(void*);
    using ResultFn = const void*(__thiscall*)(void*, uint32_t);
    uint32_t tokens[16] = {};
    __try {
        for (int i = 0; i < count; ++i) {
            uint8_t query[0x44] = {};
            memcpy(query + 0x20, segments[i].start, 12);
            memcpy(query + 0x30, segments[i].end, 12);
            const uint32_t ownerFilter = 0x20u | (uint32_t(filterA) << 8) |
                (uint32_t((exactNinja >> 9) & 0xffffu) << 16);
            const uint32_t typeFilter = uint32_t(filterB) << 8;
            memcpy(query + 0x2C, &ownerFilter, 4);
            memcpy(query + 0x40, &typeFilter, 4);
            tokens[i] = ((SubmitFn)A(0x437E20))((void*)manager, query);
        }
        ((ProcessFn)A(0x4380F0))((void*)manager);
        for (int i = 0; i < count; ++i) {
            const uint8_t* result = (const uint8_t*)((ResultFn)A(0x437E90))((void*)manager, tokens[i]);
            if (!result) return false;
            uint16_t hitCount = 0;
            memcpy(&hitCount, result + 0x0E, 2);
            hits[i].hit = hitCount != 0;
            memcpy(hits[i].point, result, 12);
            memcpy(hits[i].normal, result + 0x10, 12);
            memcpy(&hits[i].collisionObject, result + 0x3C, 4);
            hits[i].metadata = uint32_t(result[0x0C]) | (uint32_t(hitCount) << 16);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return SafeRead(exactNinja + 0x2F6, checkA) && SafeRead(exactNinja + 0x2F2, checkB) &&
        checkA == filterA && checkB == filterB &&
        ResolveOwnerHandle(A(off::StratMgrObject), identity.selfHandle, SafeReadBytes) == exactNinja;
}

bool WriteNinjaFacing(uintptr_t exactNinja, float yawRadians)
{
    if (!std::isfinite(yawRadians)) return false;
    StratIdentity identity;
    StratView current;
    if (!ReadStratIdentity(exactNinja, identity) || identity.role != StratRole::Ninja ||
        !ReadStrat(exactNinja, current) || current.selfHandle != identity.selfHandle) return false;
    float angles[4] = { current.angles[0], current.angles[1], current.angles[2], current.angles[3] };
    angles[1] = yawRadians * (4096.0f / (2.0f * 3.14159265358979323846f));
    const float half = yawRadians * 0.5f;
    const float quat[4] = { 0.0f, sinf(half), 0.0f, cosf(half) };
    return SafeWriteBytes(exactNinja + off::StratAngles, angles, sizeof(angles)) &&
        SafeWriteBytes(exactNinja + off::StratQuat, quat, sizeof(quat)) &&
        ResolveOwnerHandle(A(off::StratMgrObject), identity.selfHandle, SafeReadBytes) == exactNinja;
}

std::vector<StratView> SnapshotStrats(size_t maxStrats, bool* complete)
{
    if (complete) *complete = false;
    maxStrats = std::min<size_t>(maxStrats, 8192);
    std::vector<StratView> out;
    out.reserve(128);

    uintptr_t head = 0;
    if (!SafeRead(A(off::StratListHead), head)) return out;

    uintptr_t addr = head;
    std::vector<uintptr_t> seen;
    seen.reserve(256);
    while (addr && out.size() < maxStrats) {
        if (std::find(seen.begin(), seen.end(), addr) != seen.end()) break;
        seen.push_back(addr);

        StratView s;
        if (!ReadStrat(addr, s)) break;
        out.push_back(s);
        addr = s.next;
    }
    if (complete) *complete = addr == 0;
    return out;
}

size_t CountStrats()
{
    size_t n = 0;
    EachStrat([&](const StratView&) { ++n; });
    return n;
}

void EachStrat(const std::function<void(const StratView&)>& fn)
{
    uintptr_t addr = 0;
    if (!SafeRead(A(off::StratListHead), addr)) return;
    std::vector<uintptr_t> seen;
    size_t guard = 0;
    while (addr && guard++ < 8192) {
        if (std::find(seen.begin(), seen.end(), addr) != seen.end()) break;
        seen.push_back(addr);
        StratView s;
        if (!ReadStrat(addr, s)) break;
        fn(s);
        addr = s.next;
    }
}

// ---------------------------------------------------------------------------
//  Player tracker
// ---------------------------------------------------------------------------
PlayerTracker& PlayerTracker::Get()
{
    static PlayerTracker t;
    return t;
}

void PlayerTracker::Reset()
{
    manual_ = 0;
    auto_ = 0;
    knownClass_ = nullptr;
    knownHandle_ = 0;
    manualValid_ = false;
    mode_ = PlayerMode::Auto;
    candidates_.clear();
    candidatesComplete_ = false;
    learnStart_.clear();
    learnTotal_ = learnElapsed_ = 0.0f;
    learnDone_ = false;
    learnRanked_ = false;
    updateTimer_ = 0;
    info_.clear();
}

void PlayerTracker::SetMode(PlayerMode m)
{
    if (m != PlayerMode::Auto && m != PlayerMode::Manual && m != PlayerMode::Learning) return;
    if (mode_ != m) { auto_ = 0; manualValid_ = false; updateTimer_ = 0; }
    mode_ = m;
    if (m == PlayerMode::Learning) BeginLearning();
}

void PlayerTracker::Select(uintptr_t addr)
{
    StratView s;
    auto live = SnapshotStrats();
    const auto found = std::find_if(live.begin(), live.end(), [&](const StratView& item) { return item.addr == addr; });
    if (found != live.end() && ReadStrat(addr, s) && s.pClass == found->pClass && s.selfHandle == found->selfHandle) {
        manual_ = addr;
        mode_ = PlayerMode::Manual;
        knownClass_ = s.pClass;
        knownHandle_ = s.selfHandle;
        manualValid_ = true;
        info_ = Format("pinned %s %08X; %s", s.className[0] ? s.className : "unknown class", (unsigned)addr,
            ClassRole(s.className) == StratRole::Ninja ? "whole-body movement still requires visual confirmation" : "not exact Ninja; HL activation blocked");
        ETH_INFO("player: manual selection %08X (class %p) pos %.2f %.2f %.2f",
                 (unsigned)addr, s.pClass, s.pos[0], s.pos[1], s.pos[2]);
    } else ETH_WARN("player: refused selection %08X (not in the live strat list)", (unsigned)addr);
}

void PlayerTracker::BeginLearning()
{
    learnStart_.clear();
    learnDone_ = false;
    learnRanked_ = false;
    learnElapsed_ = 0.0f;
    learnTotal_ = 6.0f;
    manual_ = 0;
    mode_ = PlayerMode::Learning;
    EachStrat([&](const StratView& s) {
        if (s.addr) learnStart_.push_back({ s.addr, s.pClass, s.selfHandle, { s.pos[0], s.pos[1], s.pos[2] }, {} });
    });
    info_ = "Learning: close F9, idle 1s, move with WASD/arrows 3s, stop 2s. Confirm a candidate afterwards.";
    ETH_INFO("player: candidate learning started (%zu strats). Idle/move/stop; paused time is excluded.", learnStart_.size());
}

bool PlayerTracker::ReacquireExactNinja()
{
    bool complete = false;
    const auto live = SnapshotStrats(8192, &complete);
    if (!complete) return false;
    const uintptr_t exact = UniqueNinja(live);
    if (!exact) return false;
    Select(exact);
    return mode_ == PlayerMode::Manual && manual_ == exact && manualValid_;
}

void PlayerTracker::RefreshCandidates()
{
    candidates_.clear();
    CameraView cam{};
    const bool hasCamera = ReadCamera(cam) && std::isfinite(cam.x) && std::isfinite(cam.y) && std::isfinite(cam.z);

    const auto live = SnapshotStrats(8192, &candidatesComplete_);
    for (const auto& s : live) {
        float dx = s.pos[0] - cam.x;
        float dy = s.pos[1] - cam.y;
        float dz = s.pos[2] - cam.z;
        float d = hasCamera ? sqrtf(dx * dx + dy * dy + dz * dz) : -1.0f;
        if (!std::isfinite(d)) d = -1.0f;
        PlayerCandidate candidate{ s, d, s.addr == Addr() };
        if (learnRanked_) {
            auto sample = std::find_if(learnStart_.begin(), learnStart_.end(), [&](const Snapshot& p) {
                return p.addr == s.addr && p.pClass == s.pClass && p.selfHandle == s.selfHandle;
            });
            if (sample != learnStart_.end()) {
                candidate.learnScore = sample->evidence.Score();
                candidate.activeSpeed = sample->evidence.ActiveSpeed();
                candidate.idleSpeed = sample->evidence.IdleSpeed();
            }
        }
        candidates_.push_back(candidate);
    }

    std::sort(candidates_.begin(), candidates_.end(),
              [&](const PlayerCandidate& a, const PlayerCandidate& b) {
                   bool an = ClassRole(a.strat.className) == StratRole::Ninja;
                   bool bn = ClassRole(b.strat.className) == StratRole::Ninja;
                   if (an != bn) return an;
                   if (learnRanked_ && a.learnScore != b.learnScore) return a.learnScore > b.learnScore;
                  return a.distanceToCamera < b.distanceToCamera;
              });
}

void PlayerTracker::AutoPick()
{
    std::vector<StratView> live;
    for (const auto& c : candidates_) live.push_back(c.strat);
    const uintptr_t best = candidatesComplete_ ? UniqueNinja(live) : 0;
    if (!best) {
        auto_ = 0;
        knownClass_ = nullptr;
        knownHandle_ = 0;
        info_ = "No unique exact Ninja; proximity is not identity. Load a level or inspect ambiguity.";
        return;
    }
    const auto& s = *std::find_if(live.begin(), live.end(), [best](const StratView& item) { return item.addr == best; });
    const bool changed = auto_ != best || knownHandle_ != s.selfHandle || knownClass_ != s.pClass;
    auto_ = best;
    knownClass_ = s.pClass;
    knownHandle_ = s.selfHandle;
    info_ = "Unique exact Ninja detected; pin it before HL activation. Model/collision contract remains experimental.";
    if (changed) ETH_INFO("player: exact Ninja %08X class %p handle %08X", (unsigned)best, s.pClass, s.selfHandle);
}

void PlayerTracker::Update()
{
    const float dt = kTickDt;
    if (mode_ == PlayerMode::Learning) {
        const float timeStep = TimeStep();
        bool suspended = overlay::IsVisible() || !input::Window() || GetForegroundWindow() != input::Window() ||
                         !std::isfinite(timeStep) || !(timeStep > 0.0f);
        bool moving = input::KeyDown('W') || input::KeyDown('A') || input::KeyDown('S') || input::KeyDown('D') ||
                      input::KeyDown(VK_UP) || input::KeyDown(VK_DOWN) || input::KeyDown(VK_LEFT) || input::KeyDown(VK_RIGHT);
        // Baselines are kept current even while suspended. Never count paused
        // animation/camera motion as evidence of player input.
        const auto live = SnapshotStrats();
        for (auto& sample : learnStart_) {
            if (!sample.addr) continue;
            auto item = std::find_if(live.begin(), live.end(), [&](const StratView& s) {
                return s.addr == sample.addr && s.pClass == sample.pClass && s.selfHandle == sample.selfHandle;
            });
            if (item == live.end()) {
                // Once removed/reused, an old address must never inherit evidence.
                sample.addr = 0;
                sample.evidence = {};
                continue;
            }
            const StratView& now = *item;
            float dx = now.pos[0]-sample.pos[0], dy = now.pos[1]-sample.pos[1], dz = now.pos[2]-sample.pos[2];
            sample.evidence.Sample(sqrtf(dx*dx+dy*dy+dz*dz), dt, moving, suspended);
            memcpy(sample.pos, now.pos, sizeof(sample.pos));
        }
        if (!suspended) learnElapsed_ += dt;
        if (learnElapsed_ >= learnTotal_) {
            learnDone_ = learnRanked_ = true;
            learnTotal_ = 0;
            mode_ = PlayerMode::Manual; // remains unpinned: never guess the winner
            RefreshCandidates();
            if (candidates_.empty()) info_ = "Learning found no live strats. Load a level and retry.";
            else if (candidates_.front().learnScore <= 0) info_ = "No input-correlated motion found. Retry idle/move/stop with HL off; nothing was pinned.";
            else info_ = "Candidates ranked, not identified. Pin one and confirm the visible ninja with a small nudge.";
            std::string csv = "address,class,x,y,z,flags,type,active_speed,idle_speed,score,active_seconds,idle_seconds\r\n";
            for (const auto& c : candidates_) {
                auto sample = std::find_if(learnStart_.begin(), learnStart_.end(), [&](const Snapshot& p) {
                    return p.addr == c.strat.addr && p.pClass == c.strat.pClass && p.selfHandle == c.strat.selfHandle;
                });
                csv += Format("%08X,%08X,%.6f,%.6f,%.6f,%08X,%08X,%.6f,%.6f,%.6f,%.6f,%.6f\r\n",
                    (unsigned)c.strat.addr,(unsigned)(uintptr_t)c.strat.pClass,c.strat.pos[0],c.strat.pos[1],c.strat.pos[2],
                    c.strat.flags,c.strat.typeMask,c.activeSpeed,c.idleSpeed,c.learnScore,
                    sample != learnStart_.end() ? sample->evidence.activeTime : 0,
                    sample != learnStart_.end() ? sample->evidence.idleTime : 0);
            }
            SYSTEMTIME time; GetLocalTime(&time);
            std::string file = JoinPath(GetLogsDir(), Format("player-learning-%04u%02u%02u-%02u%02u%02u.csv",
                time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond));
            if (WriteStringToFile(file, csv))
                ETH_INFO("player: ranked %zu candidates (no auto-selection); evidence %s", candidates_.size(),file.c_str());
            else ETH_WARN("player: could not save learning evidence %s", file.c_str());
            for (size_t i=0; i<std::min<size_t>(8,candidates_.size()); ++i) {
                const auto& c=candidates_[i];
                ETH_INFO("player: #%zu addr %08X class %p score %.3f active %.3f idle %.3f pos %.2f %.2f %.2f",
                    i+1,(unsigned)c.strat.addr,c.strat.pClass,c.learnScore,c.activeSpeed,c.idleSpeed,
                    c.strat.pos[0],c.strat.pos[1],c.strat.pos[2]);
            }
        }
        return;
    }
    updateTimer_ -= dt;
    if (mode_ == PlayerMode::Manual) {
        if (updateTimer_ > 0.0f) return;
        updateTimer_ = 0.25f;
        StratView s;
        manualValid_ = manual_ && ReadStrat(manual_, s) && s.pClass == knownClass_ &&
            s.selfHandle == knownHandle_ && LiveAddressMatches(manual_, knownClass_, knownHandle_);
        if (!manualValid_) {
            if (manual_) ETH_WARN("player: manual strat %08X disappeared", (unsigned)manual_);
            manual_ = 0;
            info_ = "Pinned player disappeared; reselect after checking the new level.";
        }
        return; // Manual pinning never needs a full candidate snapshot.
    }
    if (updateTimer_ > 0.0f && !candidates_.empty())
        return;
    updateTimer_ = 0.25f;

    RefreshCandidates();

    if (mode_ == PlayerMode::Auto)
        AutoPick();
}

uintptr_t PlayerTracker::Addr() const
{
    return mode_ == PlayerMode::Auto ? auto_ : manual_;
}

bool PlayerTracker::Valid() const
{
    if (mode_ == PlayerMode::Manual) return manual_ && manualValid_;
    StratView s;
    if (!Addr() || !ReadStrat(Addr(), s) || s.pClass != knownClass_ || s.selfHandle != knownHandle_) return false;
    if (mode_ == PlayerMode::Auto && ClassRole(s.className) != StratRole::Ninja) return false;
    bool complete = false;
    auto live = SnapshotStrats(8192, &complete);
    if (mode_ == PlayerMode::Auto && (!complete || UniqueNinja(live) != s.addr)) return false;
    return std::any_of(live.begin(), live.end(), [&](const StratView& item) {
        return item.addr == s.addr && item.pClass == s.pClass && item.selfHandle == s.selfHandle;
    });
}

std::string PlayerTracker::DetectionInfo() const
{
    if (!info_.empty()) return info_;
    return "not detected";
}

// ---------------------------------------------------------------------------
//  Save / profile
// ---------------------------------------------------------------------------
namespace {

void* GetProg()
{
    void* profile = nullptr;
    if (!SafeRead(A(off::pProfile), profile) || !profile) return nullptr;
    if (!IsReadable((const uint8_t*)profile, 0x1EC4)) return nullptr;
    return (uint8_t*)profile + 0x14;
}

} // namespace

bool GetStatInt(int index, int& out)
{
    void* prog = GetProg();
    if (!prog || index < 0 || index >= 1024) return false;
    using Fn = int(__thiscall*)(void*, int);
    Fn fn = (Fn)A(off::Fn_GetInt);
    __try { out = fn(prog, index); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool SetStatInt(int index, int value)
{
    void* prog = GetProg();
    if (!prog || index < 0 || index >= 1024) return false;
    using Fn = void(__thiscall*)(void*, int, int);
    Fn fn = (Fn)A(off::Fn_SetInt);
    __try { fn(prog, index, value); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool GetStatBit(int index, bool& out)
{
    void* prog = GetProg();
    if (!prog || index < 0 || index >= 8192) return false;
    using Fn = int(__thiscall*)(void*, int);
    Fn fn = (Fn)A(off::Fn_GetBit);
    __try { out = fn(prog, index) != 0; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool SetStatBit(int index, bool value)
{
    void* prog = GetProg();
    if (!prog || index < 0 || index >= 8192) return false;
    using Fn = void(__thiscall*)(void*, int, int);
    Fn fn = (Fn)A(off::Fn_SetBit);
    __try { fn(prog, index, value ? 1 : 0); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

uintptr_t NativeAddr(int index)
{
    if (index < 0 || index >= off::NativeCount) return 0;
    uintptr_t p = 0;
    SafeRead(A(off::NativeTable) + 4u * index, p);
    return p;
}

} // namespace eth::game
