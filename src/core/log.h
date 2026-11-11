#pragma once
#include "precomp.h"

namespace eth {

// ---------------------------------------------------------------------------
//  Logging - thread safe, file + in-memory ring buffer (console window)
// ---------------------------------------------------------------------------
enum class LogLevel : int { Debug = 0, Info, Warn, Error };

void LogInit();
void LogShutdown();
void LogMsg(LogLevel lvl, const char* fmt, ...);
void LogVa(LogLevel lvl, const char* fmt, va_list ap);

struct LogLine {
    LogLevel    level;
    std::string text;      // "HH:MM:SS  message"
};

// Snapshot of the ring buffer (copy, safe to call from the render thread).
std::vector<LogLine> LogSnapshot();
void LogClear();

const char* LogLevelName(LogLevel lvl);
ImU32       LogLevelColor(LogLevel lvl);

} // namespace eth

#define ETH_DEBUG(...) ::eth::LogMsg(::eth::LogLevel::Debug, __VA_ARGS__)
#define ETH_INFO(...)  ::eth::LogMsg(::eth::LogLevel::Info,  __VA_ARGS__)
#define ETH_WARN(...)  ::eth::LogMsg(::eth::LogLevel::Warn,  __VA_ARGS__)
#define ETH_ERROR(...) ::eth::LogMsg(::eth::LogLevel::Error, __VA_ARGS__)
