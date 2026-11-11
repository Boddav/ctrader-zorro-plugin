#pragma once

namespace Log {

void LoadConfig(); // Read LogLevel from cTrader.ini when BrokerOpen initializes the plugin.
void Info(const char* tag, const char* fmt, ...);
void Warn(const char* tag, const char* fmt, ...);
void Error(const char* tag, const char* fmt, ...);
void Msg(const char* text);           // Show to user via BrokerMessage
void Diag(int level, const char* fmt, ...);  // Only if G.diagLevel >= level
void ToFile(const char* tag, const char* message);

} // namespace Log
