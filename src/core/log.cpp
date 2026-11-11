#include "log.h"
#include "util.h"
#include <deque>

namespace eth {

namespace {

std::mutex                g_logMutex;
FILE*                     g_logFile = nullptr;
std::deque<LogLine>       g_lines;
bool                      g_verbose = false;
constexpr size_t          kMaxLines = 3000;
constexpr long long       kMaxFileBytes = 4ll * 1024 * 1024;

const char* LevelText(LogLevel l)
{
    switch (l) {
    case LogLevel::Debug: return "DBG";
    case LogLevel::Info:  return "INF";
    case LogLevel::Warn:  return "WRN";
    default:              return "ERR";
    }
}

void RotateIfNeeded()
{
    if (!g_logFile) return;
    long long pos = _ftelli64(g_logFile);
    if (pos < kMaxFileBytes) return;
    fclose(g_logFile);
    std::string path = JoinPath(GetLogsDir(), "EthhhSDK.log");
    std::string old = JoinPath(GetLogsDir(), "EthhhSDK.old.log");
    MoveFileExA(path.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
    g_logFile = fopen(path.c_str(), "ab");
}

} // namespace

void LogInit()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    CreateDirectories(GetLogsDir());
    std::string path = JoinPath(GetLogsDir(), "EthhhSDK.log");
    g_logFile = fopen(path.c_str(), "ab");
    g_lines.clear();

    SYSTEMTIME st;
    GetLocalTime(&st);
    char header[256];
    sprintf_s(header, "===== EthhhSDK %s  session %04d-%02d-%02d %02d:%02d:%02d =====\n",
              kVersion, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    if (g_logFile) fputs(header, g_logFile);
    OutputDebugStringA(header);
}

void LogShutdown()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        fputs("===== session end =====\n", g_logFile);
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}

void LogVa(LogLevel lvl, const char* fmt, va_list ap)
{
    char msg[2048];
    vsprintf_s(msg, fmt, ap);
    LogMsg(lvl, "%s", msg);
}

void LogMsg(LogLevel lvl, const char* fmt, ...)
{
    char body[2048];
    va_list ap;
    va_start(ap, fmt);
    vsprintf_s(body, fmt, ap);
    va_end(ap);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char line[2400];
    sprintf_s(line, "%02d:%02d:%02d.%03d [%s] %s",
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, LevelText(lvl), body);

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_verbose || lvl != LogLevel::Debug) {
        OutputDebugStringA(line);
        OutputDebugStringA("\n");
    }
    if (g_logFile) {
        fputs(line, g_logFile);
        fputc('\n', g_logFile);
        fflush(g_logFile);
        RotateIfNeeded();
    }
    g_lines.push_back({ lvl, line });
    if (g_lines.size() > kMaxLines)
        g_lines.pop_front();
}

std::vector<LogLine> LogSnapshot()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    return std::vector<LogLine>(g_lines.begin(), g_lines.end());
}

void LogClear()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_lines.clear();
}

const char* LogLevelName(LogLevel lvl)
{
    switch (lvl) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info:  return "info";
    case LogLevel::Warn:  return "warn";
    default:              return "error";
    }
}

ImU32 LogLevelColor(LogLevel lvl)
{
    switch (lvl) {
    case LogLevel::Debug: return IM_COL32(130, 140, 158, 255);
    case LogLevel::Info:  return IM_COL32(214, 220, 232, 255);
    case LogLevel::Warn:  return IM_COL32(240, 190, 90, 255);
    default:              return IM_COL32(240, 100, 110, 255);
    }
}

} // namespace eth
