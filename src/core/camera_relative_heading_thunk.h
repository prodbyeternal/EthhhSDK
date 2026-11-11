#pragma once

// The getter is thiscall, three stack args, ret 0xC. Preserve its native ESI
// (next ASL instruction) and caller before any compiler-generated prologue.
// Handler is fastcall(self, unused edx, output, other, field, caller, nextPC).
// The exact same thunk is exercised by the isolated Win32 ABI regression.
#define ETH_HEADING_THUNK(name, handler) \
    __declspec(naked) uint32_t name() { \
        __asm { push esi } \
        __asm { push dword ptr [esp + 4] } \
        __asm { push dword ptr [esp + 14h] } \
        __asm { push dword ptr [esp + 14h] } \
        __asm { push dword ptr [esp + 14h] } \
        __asm { call handler } \
        __asm { ret 0Ch } \
    }
