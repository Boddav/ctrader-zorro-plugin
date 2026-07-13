# ctrader-zorro-plugin

A Zorro broker plugin for the **cTrader Open API** (WebSocket / JSON).
It lets any Zorro strategy trade a cTrader demo or live account directly,
with no MT4/MT5 bridge — the plugin talks to cTrader's Open API over a
WebSocket and exposes the standard Zorro Broker API.

## Install

1. Download `cTrader.dll` from this repository (repo root) or from the
   latest release tag.
2. Put the DLL in your Zorro `Plugin` folder.
3. (Optional) Put `cTrader.ini` next to the DLL to enable risk limits —
   see **Configuration** below.
4. Start Zorro. `cTrader` now appears in the `[Account]` scrollbox.
5. The first login opens a browser OAuth page; log in to your cTrader
   account once and the token is cached in `Plugin\oauth_token.json`.
   No Client ID/Secret is needed — they are built into the plugin.

## Configuration

### accounts.csv
Selects which cTrader account to trade. Searched in the Zorro `History/`
folder first, then in `Plugin/`. The account number is taken from here;
credentials fall back to the plugin's built-in Client ID/Secret, so a
minimal row only needs the account and `Server = Ctrader`.

### cTrader.ini (optional risk config)
Place `cTrader.ini` in the **same folder as the DLL** (`Plugin/`). It is
read once at login. Currently supported key:

```ini
; This Zorro instance may use at most this % of account equity as margin.
; When several strategies share ONE cTrader account, give each a slice so
; they cannot jointly overcommit the account (e.g. two strategies at 40 => 80%).
; 0 or the file missing = disabled (only the account-wide 90% guard applies).
MaxMarginPct = 40
```

**Why it matters:** each Zorro instance runs its own copy of the plugin and
sees the *whole* account's free margin, so the built-in per-order guard alone
cannot stop two strategies on the same account from filling it up together.
`MaxMarginPct` caps the total margin *this* instance may hold, leaving a
buffer for open-trade drawdown and preventing shared-account margin calls.

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
  `MaxMarginPct` caps the margin each instance may use, so several strategies
  sharing one account cannot jointly overcommit it (see Configuration above).

## Multiple strategies, one account

Running several Zorro instances on the **same** cTrader account is supported:
- Token refresh is mutex-serialized (no ACCESS_DENIED spiral).
- Position labels are namespaced per instance, so reconciliation never touches
  another strategy's trades.
- `MaxMarginPct` in `cTrader.ini` keeps their combined margin in check.

For full isolation, running each strategy on its **own** cTrader (sub)account
is still the cleanest option and needs none of the above safeguards.
