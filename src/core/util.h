#pragma once
#include "precomp.h"

namespace eth {

// --------------------------------------------------------------------- paths
std::string JoinPath(const std::string& a, const std::string& b);
std::string GetFileName(const std::string& path);
std::string GetFileStem(const std::string& path);
std::string GetFileExt(const std::string& path);
std::string GetParentDir(const std::string& path);
std::string NormalizePath(const std::string& path);

bool FileExists(const std::string& path);
bool DirExists(const std::string& path);
bool CreateDirectories(const std::string& path);
bool ReadFileToString(const std::string& path, std::string& out);
bool WriteStringToFile(const std::string& path, const std::string& data);
std::vector<std::string> ListDirectories(const std::string& path);
std::vector<std::string> ListFiles(const std::string& path, const char* ext = nullptr);

std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);
std::string ToLower(std::string s);
std::string Trim(const std::string& s);
std::vector<std::string> Split(const std::string& s, char sep);
bool StartsWith(const std::string& s, const char* prefix);
bool EndsWith(const std::string& s, const char* suffix);
std::string Format(const char* fmt, ...);
std::string FormatBytes(uint64_t bytes);
std::string ErrorCodeString(DWORD err);

// ------------------------------------------------------------------- module
uintptr_t ModuleBase();
uintptr_t GameAddr(uintptr_t absoluteVa);   // rebase an address from the offsets header
bool ValidateGameImage();

// ------------------------------------------------------------------- memory
bool IsReadable(const void* ptr, size_t size);
bool SafeReadBytes(uintptr_t addr, void* out, size_t size);
bool SafeWriteBytes(uintptr_t addr, const void* data, size_t size);
template <typename T> bool SafeRead(uintptr_t addr, T& out) { return SafeReadBytes(addr, &out, sizeof(T)); }
template <typename T> bool SafeWrite(uintptr_t addr, const T& v) { return SafeWriteBytes(addr, &v, sizeof(T)); }
uintptr_t FindPattern(HMODULE module, const char* pattern);

// --------------------------------------------------------------------- misc
float GetDpiScale(HWND hwnd);
std::string Sha256FileHex(const std::string& path);

// --------------------------------------------------------------------- json
// Minimal JSON value used for mod.json manifests (objects, arrays, strings,
// numbers, booleans, null). Good enough for manifests and config blobs.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    bool Parse(const std::string& text, std::string* error = nullptr);

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;

    bool IsNull() const { return type == Type::Null; }
    bool IsArray() const { return type == Type::Array; }
    bool IsObject() const { return type == Type::Object; }

    const Json* Find(const std::string& key) const;
    std::string GetString(const std::string& key, const std::string& def = "") const;
    double GetNumber(const std::string& key, double def = 0.0) const;
    bool GetBool(const std::string& key, bool def = false) const;
    std::vector<std::string> GetStringArray(const std::string& key) const;
};

} // namespace eth
