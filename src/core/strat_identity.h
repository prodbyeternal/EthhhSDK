#pragma once
#include <cstdint>
#include <cstring>
#include <limits>

namespace eth::game {

// Read-only identity policy. No session-local addresses or engine calls.
constexpr size_t kClassNameCapacity = 64;
enum class StratRole : int { Unknown = 0, Ninja = 1, Attachment = 2, Related = 3, Other = 4 };

inline StratRole ClassRole(const char* name)
{
    if (!name || !*name) return StratRole::Unknown;
    if (!strcmp(name, "Ninja")) return StratRole::Ninja;
    if (!strcmp(name, "NinjaHead") || !strcmp(name, "NinjaSword") || !strcmp(name, "NinjaRageAttachment"))
        return StratRole::Attachment;
    if (!strcmp(name, "NinjaSpeech") || !strcmp(name, "Camera")) return StratRole::Related;
    return StratRole::Other;
}

inline bool HasKnownOwnerField(const char* name)
{
    // Only these classes have corroborated +0x238 owner-handle evidence.
    return name && (!strcmp(name, "NinjaRageAttachment") || !strcmp(name, "NinjaSpeech") || !strcmp(name, "Camera"));
}

template<class Read>
bool ReadClassName(uintptr_t pClass, char (&out)[kClassNameCapacity], Read read)
{
    memset(out, 0, sizeof(out));
    if (!pClass || pClass > (std::numeric_limits<uintptr_t>::max)() - 8) return false;
    uint32_t namePtr = 0;
    if (!read(pClass + 4, &namePtr, sizeof(namePtr)) || !namePtr || namePtr > UINT32_MAX - kClassNameCapacity) return false;
    char name[kClassNameCapacity] = {};
    const bool copied = read(uintptr_t(namePtr), name, sizeof(name));
    for (size_t i = 0; i < sizeof(name); ++i) {
        if (!copied && !read(uintptr_t(namePtr) + i, &name[i], 1)) return false;
        const auto ch = static_cast<unsigned char>(name[i]);
        if (!ch) {
            uint32_t check = 0;
            if (!i || !read(pClass + 4, &check, sizeof(check)) || check != namePtr) return false;
            memcpy(out, name, i + 1);
            return true;
        }
        if (ch < 0x20 || ch > 0x7e) return false; // Reject corrupt/control/non-ASCII data.
    }
    return false; // No truncation: an unterminated name is not identity evidence.
}

template<class Read>
uintptr_t ResolveOwnerHandle(uintptr_t manager, uint32_t handle, Read read)
{
    if (!handle || !manager || manager > (std::numeric_limits<uintptr_t>::max)() - 0x1000) return 0;
    const uintptr_t slot = manager + (handle & 0x1ffu) * 8u;
    uint32_t pair[2] = {}, check[2] = {};
    if (!read(slot, pair, sizeof(pair)) || pair[0] != handle || !pair[1] ||
        !read(slot, check, sizeof(check)) || memcmp(pair, check, sizeof(pair))) return 0;
    return pair[1]; // Caller must also check live-list membership and self handle.
}

template<class Range>
uintptr_t UniqueNinja(const Range& strats)
{
    uintptr_t result = 0;
    for (const auto& strat : strats) {
        if (!strat.className[0]) return 0; // Cannot prove uniqueness with unreadable classes.
        if (ClassRole(strat.className) != StratRole::Ninja) continue;
        if (result || !strat.selfHandle) return 0;
        result = strat.addr;
    }
    return result;
}

} // namespace eth::game
