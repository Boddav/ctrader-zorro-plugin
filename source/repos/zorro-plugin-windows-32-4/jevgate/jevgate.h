#pragma once

// Jev gate: before a NEW position is opened, ask the local Jev server
// (JevServer.py, POST http://127.0.0.1:5003/gate) whether to take it.
// Configured separately from cTrader.ini, in its own subfolder: Plugin\JevGate\JevGate.ini
//
//   Mode      = log        ; off | log (ask + record, never block) | enforce (block below MinProb)
//   Tags      = Z1 Z12     ; instance tags (Zorro window title) to gate; empty = all instances
//   MinProb   = 0.50       ; enforce: reject when Jev's probability is below this
//   Port      = 5003
//   TimeoutMs = 8000
//   FailOpen  = 1          ; 1 = allow when the server is unreachable, 0 = reject
//
// Missing file = Mode off. Closing orders (opposite to an open position of this
// instance, e.g. on NFA accounts) are never gated.

struct SymbolInfo;

namespace JevGate {

// Read Plugin\JevGate\JevGate.ini (called once at login)
void LoadConfig();

// true = let the order through, false = reject it
bool Allow(const char* asset, int tradeSide, const SymbolInfo& sym);

} // namespace JevGate
