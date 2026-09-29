# Depth of Market (jevdepth)

Külön modul a cTraderJev pluginban: feliratkozik a cTrader ajánlati könyvére (DoM,
`ProtoOADepthEvent`), és a Zorro szabványos `GET_BOOK` (62) parancsán adja át.

```c
T2 Book[100];
brokerCommand(SET_SYMBOL, SymbolLive);
int n = brokerCommand(GET_BOOK, Book);   // bid-ek (fVal < 0) majd ask-ok (fVal > 0), legjobb elöl
```

- Legfeljebb 40 szint (20 bid + 20 ask), `fVol` = mennyiség alapdeviza-egységben.
- Az első hívás feliratkozik és 0-t ad; utána a szerver folyamatosan frissíti a könyvet.
- Ha 60 mp-ig nem jön frissítés (pl. újracsatlakozás után), újra feliratkozik.
- Csak élőben van adat; a Zorro History fájljai nem tárolnak DoM-ot, backtestben 0.
- Használja: `JevTradeDeep.c` (Boddav/ai-trading-advisor, `zorro/deep/`).
