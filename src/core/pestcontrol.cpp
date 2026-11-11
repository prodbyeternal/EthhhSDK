#include "pestcontrol.h"
#include "pestcontrol_policy.h"
#include "ethhhpatch_entity_reader.h"
#include "game.h"
#include "log.h"
#include "util.h"
#include <chrono>

namespace eth::patch::pest {
namespace {
constexpr uintptr_t kDeleteRequest = 0x416F10;
constexpr uint8_t kDeleteBytes[] = {0x8b,0x44,0x24,0x04,0x83,0x88,0x90,0x01,0x00,0x00,0x05,0xc3};
std::atomic<bool> g_enabled{false}, g_verified{false}, g_faulted{false};
std::mutex g_mutex;
Status g_status;
std::array<Target, kMaxRequests> g_pending{};
size_t g_pendingCount = 0;
uint32_t g_lastSearch = 0;
bool g_searched = false;
using Clock = std::chrono::steady_clock;
Layout NativeLayout() {
    return {game::A(game::off::StratMgrObject), game::A(game::off::StratListHead),
        game::A(game::off::StratListTail), game::A(game::off::StratIterCur), game::A(0x511A58)};
}
bool RequestBytesMatch() {
    uint8_t bytes[sizeof(kDeleteBytes)]{};
    uintptr_t entry = 0;
    return SafeReadBytes(game::A(kDeleteRequest), bytes, sizeof(bytes)) && !memcmp(bytes, kDeleteBytes, sizeof(bytes)) &&
        SafeRead(game::A(game::off::NativeTable) + 128 * 4, entry) && entry == game::A(kDeleteRequest);
}
bool VerifyNativeLifecycle() {
    char executable[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, executable, MAX_PATH) ||
        Sha256FileHex(executable) != "49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0" ||
        !RequestBytesMatch()) return false;
    // Checked manager branches: pending-bit path, manager cleanup, reference
    // cleanup, destructor, and free-list insertion. Do not call the asset sweep.
    struct Signature {uintptr_t va; const char* bytes; size_t count;};
    const Signature signatures[] = {
        {0x42477b,"\xf6\xc1\x01",3}, {0x424782,"\xe8\xd9\xc0\xfe\xff",5},
        {0x4247bf,"\xe8\x8c\xf4\xff\xff",5}, {0x4247d4,"\xe8\xd7\xfb\xff\xff",5},
        {0x4247df,"\xe8\x2c\x6e\x00\x00",5}, {0x4247e6,"\xe8\xf5\xf2\xfe\xff",5},
        {0x4247f7,"\x89\xbe\xa4\x18\x00\x00",6}, {0x411594,"\x89\x90\x9c\x01\x51\x00",6},
        {0x413ae4,"\xe8\x57\xf4\xff\xff",5}};
    for (const auto& s : signatures) {
        uint8_t bytes[6]{};
        if (!SafeReadBytes(game::A(s.va), bytes, s.count) || memcmp(bytes, s.bytes, s.count)) return false;
    }
    return true;
}
bool NativeRequest(const Target& target) {
    // Native 128 has exactly one consumed argument (cdecl self); no SDK pose,
    // handle, allocator, or manager-list writes. The game owns deferred teardown.
    using Fn = void(__cdecl*)(void*);
    __try { reinterpret_cast<Fn>(game::A(kDeleteRequest))(reinterpret_cast<void*>(target.address)); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace
void SetEnabled(bool enabled) {
    if (enabled && !g_verified.load(std::memory_order_acquire)) {
        const bool verified = VerifyNativeLifecycle();
        g_verified.store(verified, std::memory_order_release);
        if (!verified) ETH_WARN("PestControl: unsupported image/lifecycle; native deletion refused");
    }
    // A fault after a native request is not automatically retried in this run.
    g_enabled.store(enabled, std::memory_order_release);
}
Status ReadStatus() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Status status = g_status;
    status.enabled = g_enabled.load(std::memory_order_acquire);
    status.verified = g_verified.load(std::memory_order_acquire);
    status.faulted = g_faulted.load(std::memory_order_acquire);
    return status;
}
void OnNativeTickCompleted() {
    // Disabled path: no engine reads, allocations, config access, or locks.
    if (!g_enabled.load(std::memory_order_acquire) || !g_verified.load(std::memory_order_acquire) ||
        g_faulted.load(std::memory_order_acquire)) return;
    const Layout layout = NativeLayout();
    entity::ReadScope scope;
    const auto read = [&](uintptr_t address, void* output, size_t bytes) {
        return entity::ReadProtected(scope, address, output, bytes);
    };
    if (!Idle(layout, read)) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    size_t pending = 0;
    for (size_t i = 0; i < g_pendingCount; ++i) {
        const auto& t = g_pending[i];
        // Require a readable slot: an unreadable manager is not proof of removal.
        uint32_t pair[2]{};
        if (read(layout.manager + (t.handle & 0x1ffu) * 8, pair, sizeof(pair)) &&
            (pair[0] != t.handle || pair[1] != t.address)) ++g_status.removed;
        else g_pending[pending++] = t;
    }
    g_pendingCount = pending; g_status.pending = pending;
    const uint32_t tick = game::TickCount();
    if (g_searched && tick - g_lastSearch < kScanTicks) return;
    g_lastSearch = tick; g_searched = true;
    if (!RequestBytesMatch()) { g_faulted.store(true, std::memory_order_release); return; }
    const auto begin = Clock::now();
    const auto withinBudget = [&]() { return Clock::now() - begin < std::chrono::milliseconds(2); };
    const Plan plan = Search(true, layout, read, withinBudget);
    ++g_status.searches; g_status.detected = plan.detected; g_status.visited = plan.visited;
    g_status.listComplete = plan.complete;
    if (plan.budgetStopped) ++g_status.budgetStops;
    if (plan.complete) {
        for (size_t i = 0; i < plan.count && g_pendingCount < kMaxRequests; ++i) {
            if (!g_enabled.load(std::memory_order_acquire) || !withinBudget()) break;
            const auto& t = plan.targets[i];
            if (!Validate(layout, t, read)) { ++g_status.refused; continue; }
            if (!NativeRequest(t)) { g_faulted.store(true, std::memory_order_release); ++g_status.refused; break; }
            g_pending[g_pendingCount++] = t; ++g_status.requested;
        }
    }
    g_status.pending = g_pendingCount;
    g_status.searchMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
}
} // namespace eth::patch::pest
