# Jev kapuőr (JevGate)

Külön modul a cTraderJev pluginban (`../`, a v4.12 érintetlen). Minden **új pozíció** megnyitása előtt (`BrokerBuy2` → `BuyOrder`)
megkérdezi a helyi Jev szervert (`JevServer.py`, `POST http://127.0.0.1:5003/gate`), hogy mehet-e.
A stratégiákhoz (pl. a zárt forrású Z1+ / Z12+) nem kell hozzányúlni.

- **Beállítás:** `<Zorro>\Plugin\JevGate\JevGate.ini` (minta: `JevGate.ini` ebben a mappában).
  Ha a fájl nincs meg, a kapuőr ki van kapcsolva, a plugin pontosan úgy működik, mint eddig.
- **Napló:** `<Zorro>\Plugin\JevGate\JevGate_log.csv` — minden kérdés: idő, ablak, eszköz, irány,
  Jev esély, döntés (`allow` / `would_reject` / `reject`).
- **Zárás soha nincs szűrve:** ha a megbízás egy nyitott, ellentétes pozíciót csökkent (NFA számlán
  így zár a Zorro), a kapuőr átengedi.
- **Óránként egy kérdés** eszközönként és irányonként: ha egy stratégia az elutasított megbízást
  újraküldi, ugyanabban az órában nem kérdez újra.
- **Tiltáskor** a Zorro ablakában megjelenik: `[JevGate] Z12 EUR/USD long REJECTED: Jev p=0.41 < 0.50`,
  és a `BrokerBuy2` 0-t ad vissza (a Zorro "nem sikerült nyitni"-ként kezeli).

Javasolt sorrend: `Mode = log` néhány hétig → a `JevGate_log.csv` alapján megnézni, hogy a
`would_reject` kötések tényleg rosszabbak voltak-e → csak utána `Mode = enforce`.
