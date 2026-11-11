#pragma once
#include "ethhhpatch_entity_interpolation.h"
#include <windows.h>

namespace eth::patch::entity {
// This reader is deliberately separate from general SafeReadBytes. Region
// permissions live only for one sampling/render phase; native bytes never do.
inline bool QueryReadableRegion(uintptr_t address, ReadableRegion& out)
{
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const DWORD protection = info.Protect & 0xff;
    if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
        protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) return false;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(info.BaseAddress);
    if (info.RegionSize > UINTPTR_MAX - begin) return false;
    out = {begin, begin + info.RegionSize};
    return true;
}

inline bool CopyProtected(uintptr_t address, void* output, size_t bytes)
{
    // VirtualQuery cannot pin a native allocation, even on a cached-region hit.
    __try { memcpy(output, reinterpret_cast<const void*>(address), bytes); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline bool ReadProtected(ReadScope& scope, uintptr_t address, void* output, size_t bytes)
{
    return scope.Read(address, output, bytes, QueryReadableRegion, CopyProtected);
}
} // namespace eth::patch::entity
