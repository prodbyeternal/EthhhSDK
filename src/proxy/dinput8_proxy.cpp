// ===========================================================================
//  dinput8.dll  -  app-local proxy for I-Ninja.exe
//
//  I-Ninja statically imports DINPUT8.dll (DirectInput8Create). Windows
//  searches the application directory first for non-KnownDLLs, so this proxy
//  loads before the game's entry point. In DllMain we redirect the PE entry
//  point to Gate(), which loads EthhhSDK.dll, runs the pre-game Mod Manager,
//  restores the entry point and lets the game start.
//
//  The real Microsoft dinput8.dll is loaded from the system directory and all
//  exports are forwarded, so DirectInput behaves exactly like before.
// ===========================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <unknwn.h>
#include <cstdint>

static HMODULE g_self = nullptr;
static HMODULE g_realDinput8 = nullptr;

static HMODULE GetRealDinput8()
{
    if (g_realDinput8)
        return g_realDinput8;
    wchar_t sysDir[MAX_PATH] = {};
    UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (!n || n >= MAX_PATH)
        return nullptr;
    wchar_t path[MAX_PATH * 2];
    wsprintfW(path, L"%s\\dinput8.dll", sysDir);
    g_realDinput8 = LoadLibraryW(path);
    return g_realDinput8;
}

template <typename T>
static T Proc(const char* name)
{
    HMODULE h = GetRealDinput8();
    return h ? reinterpret_cast<T>(GetProcAddress(h, name)) : nullptr;
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
                                             LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    static Fn fn = nullptr;
    if (!fn) fn = Proc<Fn>("DirectInput8Create");
    return fn ? fn(hinst, dwVersion, riidltf, ppvOut, punkOuter) : HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
}

extern "C" HRESULT WINAPI DllCanUnloadNow(void)
{
    using Fn = HRESULT(WINAPI*)();
    static Fn fn = nullptr;
    if (!fn) fn = Proc<Fn>("DllCanUnloadNow");
    return fn ? fn() : S_FALSE;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    using Fn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    static Fn fn = nullptr;
    if (!fn) fn = Proc<Fn>("DllGetClassObject");
    return fn ? fn(rclsid, riid, ppv) : HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
}

extern "C" HRESULT WINAPI DllRegisterServer(void)
{
    using Fn = HRESULT(WINAPI*)();
    static Fn fn = nullptr;
    if (!fn) fn = Proc<Fn>("DllRegisterServer");
    return fn ? fn() : HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
}

extern "C" HRESULT WINAPI DllUnregisterServer(void)
{
    using Fn = HRESULT(WINAPI*)();
    static Fn fn = nullptr;
    if (!fn) fn = Proc<Fn>("DllUnregisterServer");
    return fn ? fn() : HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
}

extern "C" const void* WINAPI GetdfDIJoystick(void)
{
    using Fn = const void*(WINAPI*)();
    static Fn fn = nullptr;
    if (!fn) fn = Proc<Fn>("GetdfDIJoystick");
    return fn ? fn() : nullptr;
}

// ---------------------------------------------------------------------------
//  Entry point gate
// ---------------------------------------------------------------------------
namespace {

constexpr int kEntryBackupSize = 6;   // 5-byte jmp + 1 spare byte
BYTE  g_entryBackup[kEntryBackupSize] = {};
BYTE* g_entryAddr     = nullptr;
BYTE* g_entryOriginal = nullptr;      // real entry point (code), where Gate resumes
DWORD g_entryProtect  = 0;
bool  g_gateEntered   = false;

void WriteLog(const wchar_t* text)
{
    wchar_t path[MAX_PATH * 2] = {};
    GetModuleFileNameW(g_self, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash)
        return;
    wcscpy_s(slash + 1, MAX_PATH, L"EthhhSDK_proxy.log");
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(h, 0, nullptr, FILE_END);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char utf8[2048];
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8) - 1, nullptr, nullptr);
    if (n > 0) {
        char line[2304];
        int k = wsprintfA(line, "[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",
                          st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, utf8);
        DWORD written = 0;
        WriteFile(h, line, (DWORD)k, &written, nullptr);
    }
    CloseHandle(h);
}

void LogFmt(const wchar_t* fmt, ...)
{
    wchar_t buf[1600];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfW(buf, fmt, ap);
    va_end(ap);
    WriteLog(buf);
}

extern "C" void __cdecl Gate();

#pragma pack(push, 1)
struct JmpPatch { BYTE opcode; int32_t rel; };
#pragma pack(pop)

void InstallEntryPatch()
{
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (!base)
        return;

    auto* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return;
    auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return;

    g_entryAddr = base + nt->OptionalHeader.AddressOfEntryPoint;
    g_entryOriginal = g_entryAddr;
    memcpy(g_entryBackup, g_entryAddr, kEntryBackupSize);

    if (!VirtualProtect(g_entryAddr, kEntryBackupSize, PAGE_EXECUTE_READWRITE, &g_entryProtect)) {
        LogFmt(L"entry patch: VirtualProtect failed (%lu)", GetLastError());
        g_entryAddr = nullptr;
        return;
    }

    JmpPatch p;
    p.opcode = 0xE9;
    p.rel = (int32_t)((BYTE*)&Gate - (g_entryAddr + 5));
    memcpy(g_entryAddr, &p, sizeof(p));
    FlushInstructionCache(GetCurrentProcess(), g_entryAddr, kEntryBackupSize);
    LogFmt(L"entry point patched at %p -> Gate %p", g_entryAddr, &Gate);
}

void RestoreEntryPatch()
{
    if (!g_entryAddr || !g_entryProtect)
        return;
    DWORD old = 0;
    if (VirtualProtect(g_entryAddr, kEntryBackupSize, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy(g_entryAddr, g_entryBackup, kEntryBackupSize);
        FlushInstructionCache(GetCurrentProcess(), g_entryAddr, kEntryBackupSize);
        VirtualProtect(g_entryAddr, kEntryBackupSize, g_entryProtect, &old);
    }
    g_entryAddr = nullptr;
}

extern "C" void __cdecl Gate()
{
    if (g_gateEntered)
        return;
    g_gateEntered = true;

    wchar_t selfPath[MAX_PATH * 2] = {};
    GetModuleFileNameW(g_self, selfPath, MAX_PATH);
    wchar_t* slash = wcsrchr(selfPath, L'\\');
    if (slash) {
        *(slash + 1) = 0;
        wcscat_s(selfPath, MAX_PATH, L"EthhhSDK.dll");
    }

    HMODULE sdk = LoadLibraryW(selfPath);
    if (!sdk) {
        wchar_t msg[512] = {};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                       GetLastError(), 0, msg, 512, nullptr);
        LogFmt(L"failed to load %s (err %lu: %s), continuing without SDK", selfPath, GetLastError(), msg);
    } else {
        using EntryGateFn = void (*)();
        auto entryGate = (EntryGateFn)GetProcAddress(sdk, "EthhhSDK_EntryGate");
        if (entryGate) {
            LogFmt(L"EthhhSDK.dll loaded, calling entry gate");
            entryGate();
        } else {
            LogFmt(L"EthhhSDK.dll has no EthhhSDK_EntryGate export");
        }
    }

    BYTE* originalEntry = g_entryOriginal;
    RestoreEntryPatch();
    using EntryFn = void(__cdecl*)();
    EntryFn entry = (EntryFn)originalEntry;
    LogFmt(L"resuming original entry point at %p", originalEntry);
    entry();
    ExitProcess(0);
}

} // namespace

BOOL WINAPI DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = mod;
        DisableThreadLibraryCalls(mod);

        wchar_t exePath[MAX_PATH * 2] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        const wchar_t* name = wcsrchr(exePath, L'\\');
        name = name ? name + 1 : exePath;

        // Only gate the actual game; behave as a normal proxy everywhere else.
        if (_wcsicmp(name, L"I-Ninja.exe") == 0) {
            InstallEntryPatch();
        }
    }
    return TRUE;
}
