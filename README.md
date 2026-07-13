# ctrader-zorro-plugin

A Zorro broker plugin for the **cTrader Open API** (WebSocket / JSON).

## Install

1. Download `cTrader.dll` from this repository (repo root) or from the
   latest release tag.
2. Put the DLL in your Zorro `Plugin` folder.
3. Start Zorro. `cTrader` now appears in the `[Account]` scrollbox.
4. First login opens a browser OAuth flow; after that the token is cached.

## Current version: v4.12.0

### Core features
- Zorro 3.0 compatible (tested with Zorro S 3.01)
- Built-in OAuth2 Client ID/Secret — users only authenticate in the browser
- `accounts.csv` searched in `History/` first (Zorro convention), then `Plugin/`
- OAuth2 with automatic token refresh and browser-based login flow
- WebSocket connection to cTrader Open API (JSON protocol, port 5036)
- Full Broker API v2: BrokerOpen, BrokerLogin, BrokerBuy2, BrokerTrade,
  BrokerHistory2, BrokerAsset, BrokerAccount, BrokerCommand
- Market, Limit, Stop, and StopLimit orders with SL/TP modification
- M1 bar and tick history download
- Auto-reconnect with exponential backoff
- Position reconciliation on login
- Cross-currency profit/margin conversion (SymbolsForConversion API)
- Expected margin per symbol (ExpectedMargin API)
- Unrealized PnL tracking per position
- 175+ symbols (Forex, indices, commodities, crypto)
- Demo and Live account support
- C++17, Win32 x86 (winhttp, ws2_32, oleaut32, user32)

### Stability & risk features (v4.10–v4.12)
- **Orphan order cancel** — a market order that is accepted but not filled in
  time (e.g. during the daily market break) is cancelled instead of being left
  as an untracked position; if it fills mid-cancel it is adopted correctly.
- **Multi-instance token safety** — several Zorro instances can share one
  `oauth_token.json`; token refresh is serialized with a named mutex and the
  file is re-read so a single-use refresh token cannot break sibling instances.
- **Account margin guard** — rejects an order whose estimated margin exceeds
  90% of free margin instead of letting the broker margin-call the account.
- **Per-instance label namespace** — position labels carry a per-strategy tag
  (`z_{id}__{tag}`, derived from the Zorro window title) so reconciliation on a
  shared account never adopts another instance's positions.
- **Per-instance margin budget** — optional `Plugin\cTrader.ini` with
  `MaxMarginPct = 40` caps the margin each instance may use, so several
  strategies sharing one account cannot jointly overcommit it.
