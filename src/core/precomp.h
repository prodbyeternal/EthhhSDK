// ===========================================================================
//  EthhhSDK - precompiled header / shared includes
// ===========================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <chrono>

#include "imgui.h"

namespace eth {

constexpr const char* kName      = "EthhhSDK";
constexpr const char* kVersion   = "0.1.0";
constexpr const char* kWatermark = "EthhhSDK";
constexpr const char* kGameExe   = "I-Ninja.exe";

#ifndef ETH_BUILD_DATE
#define ETH_BUILD_DATE "unknown"
#endif

// Stable C ABI used by managed mods (P/Invoke) and external tools. The .def
// file is generated from these declarations by tools/gen_def.py.
#define ETH_CAPI(rettype, name, args) extern "C" __declspec(dllexport) rettype __cdecl name args

extern HMODULE g_selfModule;

std::string GetGameDir();   // directory that contains I-Ninja.exe
std::string GetSdkDir();    // <game>\EthhhSDK
std::string GetModsDir();   // <game>\EthhhSDK\mods
std::string GetConfigDir(); // <game>\EthhhSDK\config
std::string GetLogsDir();   // <game>\EthhhSDK\logs

} // namespace eth
