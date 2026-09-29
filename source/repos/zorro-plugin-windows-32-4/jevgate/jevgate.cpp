#include "../include/state.h"
#include "../include/protocol.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include "jevgate.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>

// Bar history from this same plugin (dllmain.cpp): newest bar first, returns count
DLLFUNC int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd,
                           int nTickMinutes, int nTicks, void* ticks);

namespace JevGate {

enum class Mode { Off, Log, Enforce };

static Mode s_mode = Mode::Off;
static std::vector<std::string> s_tags;   // upper-case instance tags; empty = all
static double s_minProb = 0.50;
static int s_port = 5003;
static int s_timeoutMs = 8000;
static bool s_failOpen = true;
static std::string s_dir;                 // ...\Plugin\JevGate\

// one answer per asset/side/hour, so a strategy that re-sends a rejected
// order every tick does not ask Jev again and again
struct Cached { long long hour; bool allow; double p; };
static std::map<std::string, Cached> s_cache;

static const int GATE_BARS = 200;
static const int GATE_TF_MIN = 60;

static std::string Upper(std::string s) {
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

static long long NowUnixMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (long long)(u.QuadPart / 10000ULL) - 11644473600000LL;
}

static const char* ModeName() {
    return s_mode == Mode::Enforce ? "enforce" : s_mode == Mode::Log ? "log" : "off";
}

void LoadConfig() {
    s_mode = Mode::Off;
    s_tags.clear();
    s_cache.clear();
    s_dir = std::string(G.dllDir) + "JevGate\\";

    std::ifstream f(s_dir + "JevGate.ini");
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        size_t sc = line.find(';');
        if (sc != std::string::npos) line = line.substr(0, sc);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Upper(Utils::Trim(line.substr(0, eq)));
        std::string val = Utils::Trim(line.substr(eq + 1));
        if (key == "MODE") {
            std::string v = Upper(val);
            s_mode = v == "ENFORCE" ? Mode::Enforce : v == "LOG" ? Mode::Log : Mode::Off;
        } else if (key == "TAGS") {
            std::istringstream ss(val);
            std::string t;
            while (ss >> t) {
                std::string clean;  // same sanitizing as the instance tag: alphanumerics only
                for (char ch : t) if (isalnum((unsigned char)ch)) clean += ch;
                if (!clean.empty()) s_tags.push_back(Upper(clean));
            }
        } else if (key == "MINPROB") {
            double v = atof(val.c_str());
            if (v > 0.0 && v < 1.0) s_minProb = v;
        } else if (key == "PORT") {
            int v = atoi(val.c_str());
            if (v > 0 && v < 65536) s_port = v;
        } else if (key == "TIMEOUTMS") {
            int v = atoi(val.c_str());
            if (v >= 500) s_timeoutMs = v;
        } else if (key == "FAILOPEN") {
            s_failOpen = atoi(val.c_str()) != 0;
        }
    }
    std::string tags;
    for (auto& t : s_tags) tags += t + " ";
    Log::Info("JEVGATE", "Mode=%s Tags=%s MinProb=%.2f Port=%d FailOpen=%d (instance tag '%s')",
              ModeName(), tags.empty() ? "(all)" : tags.c_str(), s_minProb, s_port,
              (int)s_failOpen, G.instanceTag.c_str());
}

static bool TagSelected() {
    if (s_tags.empty()) return true;
    std::string mine = Upper(G.instanceTag);
    return std::find(s_tags.begin(), s_tags.end(), mine) != s_tags.end();
}

// Order against an open position of this instance on the same symbol = closing (NFA)
// (trades opened this session store the Zorro asset name, reconciled ones the cTrader name)
static bool IsClosingOrder(const char* asset, const std::string& symName, int tradeSide) {
    CsLock lock(G.csTrades);
    for (auto& kv : G.trades) {
        const TradeInfo& t = kv.second;
        if (t.open && t.positionId > 0 && (t.symbol == asset || t.symbol == symName) &&
            t.tradeSide != tradeSide)
            return true;
    }
    return false;
}

// POST json to http://127.0.0.1:<port>/gate. Returns false on transport error.
static bool PostGate(const std::string& body, std::string& response) {
    HINTERNET hSession = WinHttpOpen(L"cTrader-JevGate/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    DWORD to = (DWORD)s_timeoutMs;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &to, sizeof(to));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));

    bool ok = false;
    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", (INTERNET_PORT)s_port, 0);
    HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"POST", L"/gate", NULL,
                                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0)
                                  : NULL;
    if (hRequest) {
        const wchar_t* hdr = L"Content-Type: application/json\r\n";
        if (WinHttpSendRequest(hRequest, hdr, (DWORD)-1L, (LPVOID)body.data(), (DWORD)body.size(),
                               (DWORD)body.size(), 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) {
            char buf[4096];
            DWORD n = 0;
            response.clear();
            while (WinHttpReadData(hRequest, buf, sizeof(buf), &n) && n > 0)
                response.append(buf, n);
            ok = !response.empty();
        }
        WinHttpCloseHandle(hRequest);
    }
    if (hConnect) WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

static void AppendCsv(const char* asset, const char* side, double p, const char* decision, const char* note) {
    std::string path = s_dir + "JevGate_log.csv";
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "a") != 0 || !f) return;
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0) fprintf(f, "utc,tag,asset,side,p,decision,mode,note\n");
    long long ms = NowUnixMs();
    time_t secs = (time_t)(ms / 1000);
    struct tm tmv;
    gmtime_s(&tmv, &secs);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "%s,%s,%s,%s,%.3f,%s,%s,%s\n", ts, G.instanceTag.c_str(), asset, side, p,
            decision, ModeName(), note ? note : "");
    fclose(f);
}

bool Allow(const char* asset, int tradeSide, const SymbolInfo& sym) {
    if (s_mode == Mode::Off || !TagSelected()) return true;
    const char* side = tradeSide == 1 ? "long" : "short";

    if (IsClosingOrder(asset, sym.name, tradeSide)) {
        Log::Info("JEVGATE", "%s %s: closes an open position, not gated", asset, side);
        return true;
    }

    long long nowMs = NowUnixMs();
    long long hour = nowMs / 3600000LL;
    std::string key = std::string(asset) + "|" + side;
    auto it = s_cache.find(key);
    if (it != s_cache.end() && it->second.hour == hour) {
        if (!it->second.allow)
            Log::Info("JEVGATE", "%s %s: rejected earlier this hour (p=%.2f), rejecting again",
                      asset, side, it->second.p);
        return it->second.allow;
    }

    // last GATE_BARS closed H1 bars (the newest may still be forming: drop it)
    std::vector<T6> bars(GATE_BARS + 1);
    DATE tEnd = Utils::UnixToOle(nowMs);
    DATE tStart = tEnd - (GATE_BARS + 1) * GATE_TF_MIN / 1440.0 * 1.6 - 3.0;  // weekends
    int n = BrokerHistory2((char*)asset, tStart, tEnd, GATE_TF_MIN, GATE_BARS + 1, bars.data());

    std::string body;
    if (n >= 60) {
        char num[160];
        body = "{\"asset\":\"" + std::string(asset) + "\",\"side\":\"" + side +
               "\",\"strategy\":\"" + G.instanceTag + "\",\"tf\":\"H1\",\"digits\":" +
               std::to_string(sym.digits) + ",\"bars\":[";
        bool first = true;
        for (int i = n - 1; i >= 1; i--) {  // oldest first, skip bars[0] (forming)
            const T6& b = bars[i];
            sprintf_s(num, "%s[%.5f,%.5f,%.5f,%.5f]", first ? "" : ",",
                      (double)b.fOpen, (double)b.fHigh, (double)b.fLow, (double)b.fClose);
            body += num;
            first = false;
        }
        body += "]}";
    }

    std::string resp;
    bool reached = !body.empty() && PostGate(body, resp);
    bool answered = reached && Protocol::HasField(resp.c_str(), "p") &&
                    !Protocol::HasField(resp.c_str(), "error");
    if (!answered) {
        const char* why = body.empty() ? "no bar history" : !reached ? "Jev server unreachable"
                                                                    : "Jev server error";
        bool allow = s_mode != Mode::Enforce || s_failOpen;
        Log::Warn("JEVGATE", "%s %s: %s (%.200s) -> %s", asset, side, why, resp.c_str(),
                  allow ? "allowed (FailOpen)" : "REJECTED (FailOpen=0)");
        AppendCsv(asset, side, -1.0, allow ? "allow" : "reject", why);
        return allow;
    }

    double p = Protocol::ExtractDouble(resp.c_str(), "p");
    bool pass = p >= s_minProb;
    bool allow = s_mode == Mode::Log ? true : pass;
    s_cache[key] = { hour, allow, p };
    AppendCsv(asset, side, p, pass ? "allow" : (allow ? "would_reject" : "reject"), "");

    char msg[256];
    if (!pass && s_mode == Mode::Enforce) {
        sprintf_s(msg, "[JevGate] %s %s %s REJECTED: Jev p=%.2f < %.2f", G.instanceTag.c_str(),
                  asset, side, p, s_minProb);
        Log::Error("JEVGATE", "%s", msg);
        Log::Msg(msg);  // show in the Zorro window
    } else {
        sprintf_s(msg, "[JevGate] %s %s %s %s: Jev p=%.2f (min %.2f, mode %s)", G.instanceTag.c_str(),
                  asset, side, pass ? "OK" : "would reject", p, s_minProb, ModeName());
        Log::Info("JEVGATE", "%s", msg);
        if (!pass) Log::Msg(msg);
    }
    return allow;
}

} // namespace JevGate
