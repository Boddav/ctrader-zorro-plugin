#include "../include/state.h"
#include "../include/logger.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <atomic>
#include <fstream>
#include <string>
#include "../include/utils.h"

namespace Log {

// 0=off, 1=errors, 2=warnings, 3=info (default), 4=diagnostics.
static std::atomic<int> logLevel{3};

void LoadConfig() {
    logLevel = 3;
    std::ifstream file(std::string(G.dllDir) + "cTrader.ini");
    std::string line;
    while (std::getline(file, line)) {
        line = line.substr(0, line.find_first_of(";#"));
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Utils::Trim(line.substr(0, eq));
        if (_stricmp(key.c_str(), "LogLevel") != 0) continue;
        const std::string value = Utils::Trim(line.substr(eq + 1));
        if (value.size() == 1 && value[0] >= '0' && value[0] <= '4')
            logLevel = value[0] - '0';
    }
}

static void WriteToFile(const char* tag, const char* message) {
    if (!tag || !message) return;

    CsLock lock(G.csLog);

    char path[MAX_PATH];
    sprintf_s(path, sizeof(path), "%s../Log/cTrader.log", G.dllDir);

    FILE* f = nullptr;
    if (fopen_s(&f, path, "a") == 0 && f) {
        time_t t = time(nullptr);
        struct tm tmv;
        localtime_s(&tmv, &t);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
        fprintf(f, "[%s] %s: %s\n", ts, tag, message);
        fclose(f);
    }
}

void ToFile(const char* tag, const char* message) {
    if (logLevel < 3) return;
    WriteToFile(tag, message);
}

void Info(const char* tag, const char* fmt, ...) {
    if (logLevel < 3) return;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    WriteToFile(tag ? tag : "INFO", buf);
}

void Warn(const char* tag, const char* fmt, ...) {
    if (logLevel < 2) return;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char full[4096];
    sprintf_s(full, "[WARNING] %s", buf);
    WriteToFile(tag ? tag : "WARN", full);
}

void Error(const char* tag, const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char full[4096];
    sprintf_s(full, "[ERROR] %s", buf);
    if (logLevel >= 1) WriteToFile(tag ? tag : "ERROR", full);

    if (BrokerMessage) {
        BrokerMessage(full);
    }
}

void Msg(const char* text) {
    if (!text) return;
    if (logLevel >= 3) WriteToFile("MSG", text);
    if (BrokerMessage) {
        BrokerMessage(text);
    }
}

void Diag(int level, const char* fmt, ...) {
    const int configuredLevel = logLevel.load();
    if (configuredLevel < 3 || (configuredLevel < 4 && G.diagLevel < level)) return;

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    WriteToFile("DIAG", buf);
    if (G.diagLevel >= 2 && BrokerMessage) {
        BrokerMessage(buf);
    }
}

} // namespace Log
