#include "../include/state.h"
#include "../include/protocol.h"
#include "../include/websocket.h"
#include "../include/symbols.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include "jevdepth.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

namespace JevDepth {

struct Quote { double price; double size; bool bid; };
struct Book {
    std::map<unsigned long long, Quote> quotes;  // quote id -> quote
    ULONGLONG lastUpdateMs = 0;
    ULONGLONG subscribedMs = 0;
};

static CRITICAL_SECTION s_cs;
static std::map<long long, Book> s_books;  // symbolId -> book

// resubscribe when nothing arrived for this long (e.g. after a reconnect)
static const ULONGLONG STALE_MS = 60000;

void Init() {
    InitializeCriticalSection(&s_cs);
}

static unsigned long long ParseU64(const char*& p) {
    while (*p && (*p < '0' || *p > '9')) {
        if (*p == ']') return 0;
        p++;
    }
    unsigned long long v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (unsigned long long)(*p++ - '0');
    return v;
}

void HandleDepthEvent(const char* buffer) {
    long long symbolId = Protocol::ExtractInt64(buffer, "symbolId");
    if (symbolId <= 0) return;

    // Copy new quotes out of the shared parse buffers before the next Extract call
    struct NewQ { unsigned long long id; Quote q; };
    std::vector<NewQ> added;
    {
        std::string arr = Protocol::ExtractArray(buffer, "newQuotes");
        int n = Protocol::CountArrayElements(arr.c_str());
        for (int i = 0; i < n; i++) {
            std::string e = Protocol::GetArrayElement(arr.c_str(), i);
            unsigned long long id = (unsigned long long)Protocol::ExtractInt64(e.c_str(), "id");
            long long size = Protocol::ExtractInt64(e.c_str(), "size");
            long long bid = Protocol::ExtractInt64(e.c_str(), "bid");
            long long ask = Protocol::ExtractInt64(e.c_str(), "ask");
            if (!id || size <= 0 || (bid <= 0 && ask <= 0)) continue;
            NewQ nq;
            nq.id = id;
            nq.q.bid = bid > 0;
            nq.q.price = (double)(bid > 0 ? bid : ask) / PRICE_SCALE;
            nq.q.size = (double)size / 100.0;  // cents -> base units
            added.push_back(nq);
        }
    }
    std::vector<unsigned long long> deleted;
    {
        std::string arr = Protocol::ExtractArray(buffer, "deletedQuotes");
        const char* p = arr.c_str();
        if (*p == '[') {
            p++;
            while (*p && *p != ']') {
                unsigned long long id = ParseU64(p);
                if (!id) break;
                deleted.push_back(id);
            }
        }
    }

    EnterCriticalSection(&s_cs);
    Book& b = s_books[symbolId];
    for (auto id : deleted) b.quotes.erase(id);
    for (auto& nq : added) b.quotes[nq.id] = nq.q;
    b.lastUpdateMs = Utils::NowMs();
    LeaveCriticalSection(&s_cs);
}

static bool Subscribe(long long symbolId) {
    char payload[256];
    sprintf_s(payload, "\"ctidTraderAccountId\":%lld,\"symbolId\":[%lld]", G.accountId, symbolId);
    const char* msg = Protocol::BuildMessage(Utils::NextMsgId(), PayloadType::SubscribeDepthQuotesReq, payload);
    bool ok = WebSocket::Send(msg);
    Log::Info("DEPTH", "Subscribe depth quotes symbolId=%lld: %s", symbolId, ok ? "sent" : "FAILED");
    return ok;
}

int GetBook(T2* out, int maxQuotes) {
    if (!out || maxQuotes <= 0 || !G.loggedIn || G.currentSymbol.empty()) return 0;
    SymbolInfo sym;
    if (!Symbols::GetSymbol(G.currentSymbol.c_str(), sym) || sym.symbolId <= 0) return 0;
    if (maxQuotes > JEVDEPTH_MAX_QUOTES) maxQuotes = JEVDEPTH_MAX_QUOTES;

    ULONGLONG now = Utils::NowMs();
    std::vector<Quote> bids, asks;
    bool needSub = false;

    EnterCriticalSection(&s_cs);
    Book& b = s_books[sym.symbolId];
    bool stale = b.lastUpdateMs == 0 || now - b.lastUpdateMs > STALE_MS;
    if (stale && (b.subscribedMs == 0 || now - b.subscribedMs > STALE_MS)) {
        b.quotes.clear();
        b.subscribedMs = now;
        needSub = true;
    }
    for (auto& kv : b.quotes) (kv.second.bid ? bids : asks).push_back(kv.second);
    LeaveCriticalSection(&s_cs);

    if (needSub) {
        Subscribe(sym.symbolId);
        return 0;  // the server sends the full book right after subscribing
    }

    std::sort(bids.begin(), bids.end(), [](const Quote& a, const Quote& c) { return a.price > c.price; });
    std::sort(asks.begin(), asks.end(), [](const Quote& a, const Quote& c) { return a.price < c.price; });

    DATE t = Utils::UnixToOle((long long)time(nullptr) * 1000LL);
    int half = maxQuotes / 2, n = 0;
    for (int i = 0; i < (int)bids.size() && i < half; i++, n++)
        out[n] = { t, -(float)bids[i].price, (float)bids[i].size };
    for (int i = 0; i < (int)asks.size() && i < half; i++, n++)
        out[n] = { t, (float)asks[i].price, (float)asks[i].size };
    return n;
}

} // namespace JevDepth
