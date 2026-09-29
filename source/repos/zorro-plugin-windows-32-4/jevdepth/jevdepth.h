#pragma once

// Depth of Market for Zorro: subscribes to cTrader depth quotes (ProtoOADepthEvent)
// and serves them through the standard broker command GET_BOOK (62).
//
//   lite-C:  T2 Book[100];
//            brokerCommand(SET_SYMBOL, SymbolLive);
//            int n = brokerCommand(GET_BOOK, Book);
//
// Each T2: time = now (OLE, UTC), fVal = +ask price / -bid price, fVol = size in base units.
// Bids come first (best first), then asks (best first). At most JEVDEPTH_MAX_QUOTES
// entries are written, so the caller's array must hold at least that many.
// The first call for a symbol subscribes and usually returns 0 (book not filled yet).

typedef double DATE;

#pragma pack(push, 4)
typedef struct T2 {
    DATE  time;   // GMT timestamp
    float fVal;   // price, positive for ask, negative for bid
    float fVol;   // volume / size
} T2;
#pragma pack(pop)

#define JEVDEPTH_GET_BOOK   62
#define JEVDEPTH_MAX_QUOTES 40

namespace JevDepth {

void Init();                                   // once, at DLL load
void HandleDepthEvent(const char* buffer);     // NetworkThread, payloadType 2155
int GetBook(T2* out, int maxQuotes);           // BrokerCommand(GET_BOOK) for G.currentSymbol

} // namespace JevDepth
