#include "util.h"
#include <bcrypt.h>
#include <shlwapi.h>
#include <cctype>
#include <cstdarg>

#pragma comment(lib, "shlwapi.lib")

namespace eth {

HMODULE g_selfModule = nullptr;

namespace {

std::string GetModuleDir(HMODULE mod)
{
    wchar_t path[MAX_PATH * 2] = {};
    GetModuleFileNameW(mod, path, MAX_PATH);
    std::wstring w(path);
    size_t slash = w.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        w.resize(slash);
    return WideToUtf8(w);
}

std::string g_gameDir;
std::string g_sdkDir;

void InitDirs()
{
    if (!g_gameDir.empty()) return;
    g_gameDir = GetModuleDir(nullptr);
    g_sdkDir = JoinPath(g_gameDir, "EthhhSDK");
}

} // namespace

std::string GetGameDir()   { InitDirs(); return g_gameDir; }
std::string GetSdkDir()    { InitDirs(); return g_sdkDir; }
std::string GetModsDir()   { InitDirs(); return JoinPath(g_sdkDir, "mods"); }
std::string GetConfigDir() { InitDirs(); return JoinPath(g_sdkDir, "config"); }
std::string GetLogsDir()   { InitDirs(); return JoinPath(g_sdkDir, "logs"); }

// --------------------------------------------------------------------- paths
std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '\\' || last == '/') return a + b;
    return a + "\\" + b;
}

std::string GetFileName(const std::string& path)
{
    size_t p = path.find_last_of("\\/");
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::string GetFileStem(const std::string& path)
{
    std::string n = GetFileName(path);
    size_t p = n.find_last_of('.');
    return p == std::string::npos ? n : n.substr(0, p);
}

std::string GetFileExt(const std::string& path)
{
    std::string n = GetFileName(path);
    size_t p = n.find_last_of('.');
    return p == std::string::npos ? std::string() : ToLower(n.substr(p + 1));
}

std::string GetParentDir(const std::string& path)
{
    size_t p = path.find_last_of("\\/");
    return p == std::string::npos ? std::string() : path.substr(0, p);
}

std::string NormalizePath(const std::string& path)
{
    std::string p = path;
    for (auto& c : p) if (c == '/') c = '\\';
    return p;
}

bool FileExists(const std::string& path)
{
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirExists(const std::string& path)
{
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool CreateDirectories(const std::string& path)
{
    if (path.empty() || DirExists(path)) return true;
    std::string parent = GetParentDir(path);
    if (!parent.empty() && parent != path && !DirExists(parent))
        CreateDirectories(parent);
    if (CreateDirectoryA(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool ReadFileToString(const std::string& path, std::string& out)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 64 * 1024 * 1024) {
        CloseHandle(h);
        return false;
    }
    out.resize((size_t)size.QuadPart);
    DWORD read = 0;
    BOOL ok = out.empty() || ReadFile(h, out.data(), (DWORD)out.size(), &read, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    out.resize(read);
    return true;
}

bool WriteStringToFile(const std::string& path, const std::string& data)
{
    CreateDirectories(GetParentDir(path));
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = data.empty() || WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
    CloseHandle(h);
    return ok && written == data.size();
}

std::vector<std::string> ListDirectories(const std::string& path)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    std::string pat = JoinPath(path, "*");
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
            out.push_back(fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> ListFiles(const std::string& path, const char* ext)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    std::string pat = JoinPath(path, "*");
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string name = fd.cFileName;
        if (ext && GetFileExt(name) != ToLower(ext)) continue;
        out.push_back(name);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string Trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> Split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t p = s.find(sep, start);
        out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return out;
}

bool StartsWith(const std::string& s, const char* prefix)
{
    size_t n = strlen(prefix);
    return s.size() >= n && memcmp(s.data(), prefix, n) == 0;
}

bool EndsWith(const std::string& s, const char* suffix)
{
    size_t n = strlen(suffix);
    return s.size() >= n && memcmp(s.data() + s.size() - n, suffix, n) == 0;
}

std::string Format(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsprintf_s(buf, fmt, ap);
    va_end(ap);
    return buf;
}

std::string FormatBytes(uint64_t bytes)
{
    if (bytes < 1024) return Format("%llu B", (unsigned long long)bytes);
    if (bytes < 1024ull * 1024) return Format("%.1f KB", bytes / 1024.0);
    if (bytes < 1024ull * 1024 * 1024) return Format("%.1f MB", bytes / (1024.0 * 1024.0));
    return Format("%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
}

std::string ErrorCodeString(DWORD err)
{
    char* buf = nullptr;
    DWORD n = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, err, 0, (LPSTR)&buf, 0, nullptr);
    std::string s = n ? Trim(std::string(buf, n)) : Format("error %lu", err);
    if (buf) LocalFree(buf);
    return s;
}

// ------------------------------------------------------------------- module
uintptr_t ModuleBase()
{
    static uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    return base;
}

uintptr_t GameAddr(uintptr_t absoluteVa)
{
    return ModuleBase() + (absoluteVa - 0x400000);
}

bool ValidateGameImage()
{
    auto* dos = (IMAGE_DOS_HEADER*)ModuleBase();
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = (IMAGE_NT_HEADERS*)(ModuleBase() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) return false;
    return true;
}

// ------------------------------------------------------------------- memory
bool IsReadable(const void* ptr, size_t size)
{
    if (!ptr || size == 0) return false;
    MEMORY_BASIC_INFORMATION mbi = {};
    uintptr_t addr = (uintptr_t)ptr;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return addr < regionEnd && size <= regionEnd - addr;
}

bool SafeReadBytes(uintptr_t addr, void* out, size_t size)
{
    if (!addr || !out || !size) return false;
    if (!IsReadable((const void*)addr, size)) return false;
    // VirtualQuery does not pin allocations. An object can disappear between
    // the protection check and the copy (especially during level transitions).
    __try {
        memcpy(out, (const void*)addr, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool SafeWriteBytes(uintptr_t addr, const void* data, size_t size)
{
    if (!addr || !data || !size) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (addr >= end || size > end - addr) return false;
    const DWORD protection = mbi.Protect & 0xff;
    const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    if (writable) {
        __try { memcpy((void*)addr, data, size); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY)
            FlushInstructionCache(GetCurrentProcess(), (void*)addr, size);
        return true;
    }
    DWORD old = 0;
    if (!VirtualProtect((void*)addr, size, PAGE_EXECUTE_READWRITE, &old))
        return false;
    bool copied = false;
    __try { memcpy((void*)addr, data, size); copied = true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { copied = false; }
    DWORD ignored = 0;
    VirtualProtect((void*)addr, size, old, &ignored);
    if (!copied) return false;
    FlushInstructionCache(GetCurrentProcess(), (void*)addr, size);
    return true;
}

uintptr_t FindPattern(HMODULE module, const char* pattern)
{
    auto* base = (uint8_t*)module;
    if (!base) return 0;

    auto* dos = (IMAGE_DOS_HEADER*)base;
    auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    const char* p = pattern;

    std::vector<int> bytes;
    bool wildcard = false;
    std::vector<bool> wild;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (*p == '?') {
            bytes.push_back(0);
            wild.push_back(true);
            wildcard = true;
            while (*p == '?') ++p;
        } else {
            char hex[3] = { p[0], p[1], 0 };
            bytes.push_back((int)strtol(hex, nullptr, 16));
            wild.push_back(false);
            p += 2;
        }
    }
    if (bytes.empty()) return 0;

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& s = sec[i];
        if (!(s.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uint8_t* data = base + s.VirtualAddress;
        size_t size = s.Misc.VirtualSize;
        if (bytes.size() > size) continue;
        for (size_t off = 0; off + bytes.size() <= size; ++off) {
            bool match = true;
            for (size_t k = 0; k < bytes.size(); ++k) {
                if (!wild[k] && data[off + k] != (uint8_t)bytes[k]) { match = false; break; }
            }
            if (match) return (uintptr_t)(data + off);
        }
    }
    return 0;
}

// --------------------------------------------------------------------- misc
float GetDpiScale(HWND hwnd)
{
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static auto getDpi = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow");
    if (getDpi && hwnd) {
        UINT dpi = getDpi(hwnd);
        if (dpi) return dpi / 96.0f;
    }
    HDC dc = hwnd ? GetDC(hwnd) : GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    if (hwnd) ReleaseDC(hwnd, dc); else ReleaseDC(nullptr, dc);
    return dpi > 0 ? dpi / 96.0f : 1.0f;
}

std::string Sha256FileHex(const std::string& path)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string result;
    DWORD hashLen = 0, cb = 0;
    std::vector<uint8_t> hashObj, digest;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
        BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0) == 0) {
        digest.resize(hashLen);
        if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&cb, sizeof(cb), &cb, 0) == 0) {
            hashObj.resize(cb);
            if (BCryptCreateHash(alg, &hash, hashObj.data(), (ULONG)hashObj.size(), nullptr, 0, 0) == 0) {
                std::vector<uint8_t> buf(64 * 1024);
                DWORD read = 0;
                while (ReadFile(h, buf.data(), (DWORD)buf.size(), &read, nullptr) && read > 0)
                    BCryptHashData(hash, buf.data(), read, 0);
                if (BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) == 0) {
                    static const char* hex = "0123456789abcdef";
                    result.reserve(digest.size() * 2);
                    for (uint8_t b : digest) { result.push_back(hex[b >> 4]); result.push_back(hex[b & 15]); }
                }
                BCryptDestroyHash(hash);
            }
        }
    }
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(h);
    return result;
}

// --------------------------------------------------------------------- json
namespace {

struct JsonParser {
    const std::string& s;
    size_t i = 0;
    std::string err;

    explicit JsonParser(const std::string& text) : s(text) {}

    void SkipWs()
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    }

    bool Fail(const char* what)
    {
        if (err.empty()) err = Format("%s at offset %zu", what, i);
        return false;
    }

    bool ParseValue(Json& out)
    {
        SkipWs();
        if (i >= s.size()) return Fail("unexpected end");
        char c = s[i];
        if (c == '{') return ParseObject(out);
        if (c == '[') return ParseArray(out);
        if (c == '"') { out.type = Json::Type::String; return ParseString(out.string); }
        if (c == 't' || c == 'f') {
            if (s.compare(i, 4, "true") == 0) { i += 4; out.type = Json::Type::Bool; out.boolean = true; return true; }
            if (s.compare(i, 5, "false") == 0) { i += 5; out.type = Json::Type::Bool; out.boolean = false; return true; }
            return Fail("bad literal");
        }
        if (c == 'n') {
            if (s.compare(i, 4, "null") == 0) { i += 4; out.type = Json::Type::Null; return true; }
            return Fail("bad literal");
        }
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
        return Fail("unexpected character");
    }

    bool ParseString(std::string& out)
    {
        if (s[i] != '"') return Fail("expected string");
        ++i;
        out.clear();
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= s.size()) return Fail("bad escape");
                char e = s[i++];
                switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (i + 4 > s.size()) return Fail("bad unicode escape");
                    unsigned cp = (unsigned)strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                    i += 4;
                    // UTF-8 encode (BMP only; manifests don't need surrogate pairs)
                    if (cp < 0x80) out.push_back((char)cp);
                    else if (cp < 0x800) {
                        out.push_back((char)(0xC0 | (cp >> 6)));
                        out.push_back((char)(0x80 | (cp & 0x3F)));
                    } else {
                        out.push_back((char)(0xE0 | (cp >> 12)));
                        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back((char)(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: return Fail("bad escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return Fail("unterminated string");
    }

    bool ParseNumber(Json& out)
    {
        size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) ++i;
        char* end = nullptr;
        out.number = strtod(s.substr(start, i - start).c_str(), &end);
        out.type = Json::Type::Number;
        return true;
    }

    bool ParseArray(Json& out)
    {
        out.type = Json::Type::Array;
        ++i;
        SkipWs();
        if (i < s.size() && s[i] == ']') { ++i; return true; }
        while (true) {
            Json v;
            if (!ParseValue(v)) return false;
            out.array.push_back(std::move(v));
            SkipWs();
            if (i >= s.size()) return Fail("unterminated array");
            if (s[i] == ',') { ++i; continue; }
            if (s[i] == ']') { ++i; return true; }
            return Fail("expected , or ]");
        }
    }

    bool ParseObject(Json& out)
    {
        out.type = Json::Type::Object;
        ++i;
        SkipWs();
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        while (true) {
            SkipWs();
            std::string key;
            if (i >= s.size() || !ParseString(key)) return Fail("expected object key");
            SkipWs();
            if (i >= s.size() || s[i] != ':') return Fail("expected :");
            ++i;
            Json v;
            if (!ParseValue(v)) return false;
            out.object[key] = std::move(v);
            SkipWs();
            if (i >= s.size()) return Fail("unterminated object");
            if (s[i] == ',') { ++i; continue; }
            if (s[i] == '}') { ++i; return true; }
            return Fail("expected , or }");
        }
    }
};

} // namespace

bool Json::Parse(const std::string& text, std::string* error)
{
    JsonParser p(text);
    if (!p.ParseValue(*this)) {
        if (error) *error = p.err;
        return false;
    }
    p.SkipWs();
    if (p.i != text.size()) {
        if (error) *error = Format("trailing data at offset %zu", p.i);
        return false;
    }
    return true;
}

const Json* Json::Find(const std::string& key) const
{
    auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::string Json::GetString(const std::string& key, const std::string& def) const
{
    const Json* v = Find(key);
    return v && v->type == Type::String ? v->string : def;
}

double Json::GetNumber(const std::string& key, double def) const
{
    const Json* v = Find(key);
    return v && v->type == Type::Number ? v->number : def;
}

bool Json::GetBool(const std::string& key, bool def) const
{
    const Json* v = Find(key);
    if (!v) return def;
    if (v->type == Type::Bool) return v->boolean;
    if (v->type == Type::Number) return v->number != 0;
    return def;
}

std::vector<std::string> Json::GetStringArray(const std::string& key) const
{
    std::vector<std::string> out;
    const Json* v = Find(key);
    if (!v || v->type != Type::Array) return out;
    for (const auto& e : v->array)
        if (e.type == Type::String) out.push_back(e.string);
    return out;
}

} // namespace eth
