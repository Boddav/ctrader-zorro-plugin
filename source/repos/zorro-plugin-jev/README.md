# cTraderJev — a cTrader plugin Jev-es változata

A v4.12 plugin (`../zorro-plugin-windows-32-4/`) másolata, két külön modullal kiegészítve.
**A v4.12 érintetlen**, a kettő egymás mellett használható.

| | v4.12 | cTraderJev |
|---|---|---|
| Projekt | `zorro-plugin-windows-32-4/cTrader.sln` | `zorro-plugin-jev/cTraderJev.sln` |
| DLL | `cTrader.dll` | `cTraderJev.dll` |
| Zorro [Account] név | `cTrader` | `cTraderJev` |
| Napló | `Plugin\cTrader.log` | `Plugin\cTraderJev.log` |
| Jev kapuőr | – | `jevgate/` → `Plugin\JevGate\JevGate.ini` |
| Depth of Market (`GET_BOOK`) | – | `jevdepth/` |

Közös marad (ugyanabban a `Plugin` mappában): `accounts.csv`, `oauth_token.json` (a tokenfrissítés
a két DLL között is zárolt), `cTrader.ini` (`MaxMarginPct`).

## Fordítás
Visual Studio: `cTraderJev.sln` → Release | Win32 → Build → `Release\cTraderJev.dll`
→ másold a Zorro `Plugin` mappájába a `cTrader.dll` mellé.

## Használat
`accounts.csv`-ben egy új sor, a `Plugin` oszlopban `cTraderJev.dll` (a többi oszlop mint a
meglévő cTrader sorodnál), pl. `Name` = `JevDemo`. A Zorróban ezt a sort választva a Jev-es plugin
fut; a többi sor továbbra is a v4.12 `cTrader.dll`-t használja.

- Jev kapuőr: `jevgate/README.md` (alapból ki van kapcsolva, amíg nincs `Plugin\JevGate\JevGate.ini`)
- Depth of Market: `jevdepth/README.md`
