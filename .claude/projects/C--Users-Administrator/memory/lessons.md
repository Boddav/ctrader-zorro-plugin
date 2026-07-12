# Lessons Learned — Tanár-Diák ML Trading

## SOHA NE CSINÁLD (bizonyítottan bukott)
1. **ML irány-predikció** regime/volatility feature-ökkel (r < 0.08) — v9.12, v9.13
2. **7 asset WFO** (84 optimize param) — "no parameters at walk N" hiba — v7
3. **ICT feature-ök** (FVG, OTE, sweep, BOS) — r < 0.05 korreláció — v8+ICT
4. **Multi-model ensemble filter** — zajt erősít, nem csökkent — v9.12
5. **Tanár újratréning** backup nélkül — PF 1.59 → 1.01 lett
6. **TREND-CLOSE tanárban** — PF 1.09 → 1.04 (diákban OK mert ML filter előszűr)
7. **Auto algo_vote (slider 4)** — filter modell nem elég pontos algo választásra
8. **Diák optimize()** set(PARAMETERS) — .par algo-nként szétválik, szerver+Train instabil → PF 0.78
9. **optimize("Name", ...)** string paraméterrel — z3 Zorro NEM támogatja, "ARRAY to DOUBLE" hiba. Csak `optimize(val, min, max, step)` működik (4 arg, név nélkül)
10. **optimize() manuális for+asset() loop-ban** — Zorro nem kezeli per-asset .par-t. MINDIG `while(asset(loop(...)))` + `algo()` kell (tanár mintájára)
11. **GBP/USD z3-ban** — 2026-03-13 óta VAN 2019 history (letöltve). Korábban nem volt!
14. **`for()` asset loop + `asset()` fail → irregular optimize**: Ha `asset()` return 0-t ad (nincs history), `continue` kihagyja az `optimize()` hívást → Zorro "irregular optimize calls". MEGOLDÁS: registryben CSAK history-val rendelkező assetek! `optimize()` MINDEN registrált asset-re KELL fusson, inaktívak: optimize+series fut, de `if(!assetActive) continue;` a series UTÁN
15. **z3 2019 history**: EURUSD, GBPUSD, USDJPY, USDCHF VAN. USDCAD, XAUUSD, AUDUSD, EURCHF NINCS (2026-03-13)
16. **`for()` asset loop SOHA optimize()-tal**: Még ha `asset()` sikeres is, a `for()` loop `continue`-val kihagyhat iterációkat → "irregular optimize calls". EGYETLEN működő minta: `while(asset(loop("A","B","C")))` + `algo()` + `optimize()`. Inaktív assetek: optimize+series fut, `if(!assetActive) continue;` a series UTÁN
17. **CDL candlestick eredmények**:
    - H1: PF 0.97 (nem prediktív)
    - H4 szűrők nélkül: PF 0.92, 3175 trade (random)
    - H4 + trend+session: **PF 1.29, CAGR 21.36%, 734 trade** ← LEGJOBB
    - GBP/USD PF 1.53, USD/JPY SHORT PF 0.76 (gyenge)
    - CDL signal önmagában NEM elég — trend filter (price vs SMA200) KELL
    - Túl sok szűrő (S/R + strength + ADX + RSI) → 0 trade H4-en
    - Minimal szűrő = legjobb: `cdlDir + trendUp/Dn + inSession`
18. **TA-Lib CDL vs saját pattern kód**: Beépített CDL függvények (Engulfing, Hammer, MorningStar stb.) JOBBAK mint saját swing/neckline detection. Kevesebb kód (100 vs 900 sor), kevesebb bug, tesztelt
19. **Optimize step méret KRITIKUS**: Túl kicsi step (0.25) × sok param → 0 trade WFO-ban. Nagyobb step (0.5-1.0) + kevesebb param (5-7/asset) konvergál
12. **CDL candlestick pattern-ek H1-en** — PF 0.97, nem prediktívek. H4-en PF 1.21 — CDL D1/H4-re lett tervezve, NEM H1-re!
13. **Új asset hozzáadás ELŐTT** mindig ellenőrizd a history elérhetőséget! `asset("X"); plot()` teszt MIELŐTT optimize loop-ba raknád

## MINDIG MŰKÖDIK
1. **ML = szűrő (GO/SKIP)**, nem irány-prediktor — PF 1.02 → 1.60
2. **Channel = irány**, ML = minőség-szűrő
3. **3 asset maximum** WFO-hoz (36 param, konvergál)
4. **Backup CSV mentés** minden tréning előtt!
5. **algo() szeparáció + FCFS** — SMA és CH nem zavarják egymást
6. **Filter threshold**: SMA=0.45, CH=0.40 (empirikus)
7. **ATR-alapú küszöbök** PIP helyett (multi-asset kompatibilis)
8. **Exit logika MINDIG a skipBar ELŐTT** — nyitott pozíció zárása session-ön kívül is kell
9. **Position RECOVERY** restart után: Bar==1 + TRADEMODE → `NumOpenLong/Short` alapú state recovery
10. **`year(0) > 2000`** lookback szűréshez — megbízhatóbb mint `Bar > LookBack`
11. **Multi-asset optimize**: `while(asset(loop(...)))` + `strstr(Asset,...)` + `algo("X")` + `optimize()` — ez a HELYES Zorro minta (tanár mintájára)
12. **Bilateral pattern target**: EGYSZER számolni (`patTargetDone` flag) — ha entry szűrők blokkolják, ne számolja újra, mert a raw height felülíródik → exponenciális hiba (TP=-15 milliárd)

## WFO LIMITEK
- Max ~40 optimize param összesen (3 asset × 12 + overhead)
- 10 WFO ciklus standard
- DataSplit 80/20
- LookBack 900 (kell Hurst(100), MMI(200) -hoz)

## KÓDHIBÁK (audit 2026-03-07)
1. **Current_State eltérés**: tanár `(Balance-InitialBalance)/Capital`, diák `(Equity-Capital)/Capital` → kis distribution shift
2. **CH LifeTime**: tanár optimize(), diák hardcode 0 → /predict feleslegesen prediktálja
3. **algo_vote túl megengedő**: mindkét irány OR → tévesen engedélyez

## PRODUCTION HIBÁK
1. **WinHTTP 12002 timeout → Zorro befagy** (2026-03-09): cTrader WebSocket Send failed → `disconnected`, Zorro nem kapott több adatot, 14 órán át nem logolt. Plugin auto-reconnect NEM indult el (vagy kimerítette a 10 próbát). Stratégia belső state (layersLong/Short, FCFS) nullázódik restart-nál → dupla pozíció nyílhat.
   - **Fix**: INITRUN után Bar==1-nél `NumOpenLong`/`NumOpenShort` alapú layer recovery
   - **Fix**: ML szerver fallback — `mlReady` marad 1 ha volt korábbi predikció
   - **Fix**: `SET_ORDERTEXT` minden entry előtt → cTrader label tartalmazza a script nevét
2. **ML szerver timeout ≠ cTrader disconnect**: `http_transfer()` (Zorro built-in) timeout nem azonos a WebSocket plugin timeout-jával — külön kezelendő!
3. **skipBar continue kihagyja az exit logikát** (2026-03-10): CH EXIT blokk a `skipBar` UTÁN volt → session-ön kívül (hr < sessStart) soha nem futott az exit_check nyitott pozíciókra. Fix: CH EXIT blokkot a skipBar ELÉ mozgatni.
4. **`Bar > LookBack` feltétel nem működik kevés adattal**: Ha nincs 900 bar adat (pl. hétvége után), `Bar` sosem éri el a `LookBack`-et → exit_check és CSV rögzítés nem fut. Fix: `year(0) > 2000` (lookback bar-ok 1999-es dátummal futnak).
5. **TradeBars negatív reconciled trade-eknél**: Reconciled pozíciók `TradeBarOpen`-je a valódi nyitás bar-ja, ami a lookback előtt volt → `TradeBars` negatív. Fix: `if(barsHeld < 1) barsHeld = 50;`
6. **Zorro restart "Close all trades?" dialog**: Véletlenül "Yes"-re nyomva lezárja az összes nyitott pozíciót. NINCS visszavonás. → MINDIG figyelmesen olvass restart-nál!

## ZORRO 3.0 (z7 3.0) TANULSÁGOK
1. **`all_algos` macro NEM működik** lite-C-ben `{ }` blokkkal — fordító lefagy. Kézi `algo("SMA"); algo("CH"); algo("CDL")` kell
2. **`strvarCSV()` MŰKÖDIK** — CSV parse 30 sor kézi strchr → 3 sor natív
3. **`NOFLAT` flag** — NEM gyorsító, hanem flat bar gap-kezelő (series nem tolja)
4. **8 optimize() param = ~500,000 kombináció** → Train lefagy. Max 4 param egyszerre!
5. **`strncpy()` NEM létezik** lite-C-ben — `sprintf("%.30s", src)` + `strchr('"')` vágás
6. **`int` deklaráció for közepén** NEM megengedett lite-C-ben — blokk elejére kell
7. **Conditional `series()` hívás TILOS** — "Wrong series usage" hiba. Series MINDEN bar-on KELL fusson (shared section)
8. **z3.0 loop() algo-kkal**: `while(algo(loop("SMA","CH","CDL")))` + `if(Algo == "SMA")` — per-algo optimize(), de eltérő logikájú algóknál nehézkes
9. **z3.0 phantom()**: equity curve trading, NEM SKIP trade tracking — más célra való
10. **z3.0 evaluate()**: test végén automatikus statisztika — custom PF/SR report
11. **z3.0 optimalf()**: per-asset/algo OptimalF lot sizing — `set(FACTORS)` kell Train-ben
12. **z3.0 file_appendCSV()**: okos append (egyező mező → felülírás, nem duplikálás)
13. **CDL session exit ÖLDÖSTE a trade-eket** — PF 0.83, 100% exit 20:00-kor, 0 trade érte el TP-t. Fix: session exit törlése CDL-ről (overnight OK, H4 pattern napokig tarthat)
14. **CDL StopFactor kompenzáció KELL** — `Stop = stopDist / StopFactor` hogy a tényleges stop az legyen amit terveztünk
15. **Chart pattern (FTMO cikk) ≠ Candlestick pattern (TA-Lib CDL)**: H&S, Double Top, Triangle = geometriai formációk (hetek), Engulfing, Hammer = 1-3 bar gyertyák. TELJESEN MÁS!
16. **Python `tradingpatterns` könyvtár → lite-C port SIKERÜLT**: getChartPattern() natív, nincs HTTP overhead, Train-ben is megy, optimize()-ható
17. **Chart pattern túl sok jelet ad** window=5-tel — 22,792 detekció, 2815 trade, PF romlik. Window növelés és strength filter kell
18. **CDL PF evolúció**: 0.83 (session exit) → 0.90 (overnight) → 1.05 (SMA+CH+CDL) → **1.09** (strength≥2 + BE trail) → chart pattern tesztelés lezárva
19. **Chart pattern (swing-based) H4 BUKÁS**: Helyes swing detection + breakout confirmation + 4 param optimize → PF 1.00 (breakeven). Csak DOUBLE_TOP/BOTTOM triggerel, H&S/Triangle/Wedge túl ritka. H1-en darál (kicsi ATR). A chart pattern önmagában NEM prediktív forex-en.
20. **CDL_Train standalone eredmények (2026-03-14)**:
    - H4 default (candlestick+pattern): PF 1.27, 652 trade, CAGR 43% — DE DD 45%, lot sizing elszáll
    - H4 optimize (pattern only): PF 1.00, 2643 trade — breakeven
    - H4 cooldown+fix lot: PF 1.03, 3128 trade — alig profitábilis
    - H1: darál, gyors kilépő, nem prediktív
    - H1 peak()/valley(): PF 0.92, 10246 trade — túl sok jel, veszteséges
    - H1 pyramid+trail: tesztelés alatt
    - **Tanulság**: Chart pattern forex-en NEM működik önállóan. ML szűrő nélkül breakeven.
21. **Zorro 3.0 (z7 3.0) új funkciók**: `peak()`/`valley()` swing detection, `phantom()` equity curve, `event()`, `strvarCSV()`, `file_appendCSV()`, named `optimize("Name",...)`, `Train` makró, `PEAK` flag objective()-hez, `MAE()`, `LRSI()`, `BOOTSTRAP`
22. **`objective()` KELL `var` return type** — `function` = `int` z3.0-ban, `var objective()` explicit!
23. **`set(PARAMETERS)` nélkül Test mód .par nélkül is fut** — `if(Train) set(PARAMETERS);` a megoldás
24. **optimize() multi-asset**: `while(asset(loop(...)))` + `algo()` + `optimize()` az EGYETLEN helyes minta. `for()` loop-ban SOHA (lessons #16 megerősítve)
25. **BarPeriod=240 + TimeFrame=4 = D1** — NEM H4! Ha BarPeriod már H4 natívan, TimeFrame=1 kell
26. **Session filter H4-en értelmetlen** — `hour()` csak 0,4,8,12,16,20 értéket ad, sessionEnd=(hr>=19) sosem igaz
27. **Egyes algo-k csak LONG vagy SHORT irányban profitábilisak** — per-asset + per-direction szűrő kell
28. **predict(CROSSOVER) + SMA + regime = PF 1.19** (2026-03-14, CDL_Train.c):
    - `predict(CROSSOVER, SMA_Fast-SMA_Slow, 10, 0)` — crossover előrejelzés
    - Pyramid (4 layer, cooldown 6 bar, növekvő lot)
    - Regime filter: ADX>25 + MMI<75 → PF 1.12→1.19
    - 3 asset (EUR/USD, GBP/USD, USD/JPY) — USD/CHF rontott
    - Session filter NEM javít (éjszakai jelek is jók)
    - RSI szűrő NEM javít (PF 1.19→1.13)
    - Default params (Fast=20, Slow=50, RR=2.0, PredW=10) JOBBAK mint optimize-oltak
    - Fix 1 lot → CAGR 0.92%, lot sizing-gal javulna
    - **Következő**: WFO Train, equity lot sizing, live teszt
29. **LRSI szűrő tesztelés (2026-03-15, CDL_Train.c)**:
    - **Zóna filter** (0.2 < lrsi < 0.8): PF 0.96, 231 trade — BUKÁS. Mean-reversion indikátor trend-following-ra = ellentmondás. A legjobb trend belépéseket szűri ki.
    - **Trend confirmation** (long: lrsi>0.5, short: lrsi<0.5): nem javított
    - **Kontra filter** (long: lrsi<0.5, short: lrsi>0.5): **PF 1.46**, 152 trade — jobb minőség (avg +1.8p vs +0.9p), de túl kevés trade (CAGR 0.14%)
    - optimize() → 0.543 ≈ default 0.5 — nincs nyereség
    - **Tanulság**: LRSI kontra = "buy the dip" PF-et javít, de trade számot elpusztítja. NEM éri meg trend-following-ra.
    - **LRSI NaN fallback**: range 0-1, tehát `lrsi = 0.5` kell, NEM 50!
30. **H&S vizualizáció tanulságok (2026-03-15)**:
    - W=30 H1-en = túl érzékeny, szinte minden bar-on jel (30 óra ≈ 1.25 nap — mindig van 3 swing)
    - W=60+ kell valódi H&S formációkhoz H1-en
    - `cpName[0] == 'H'` / `'I'` szűrés → H&S vs Double Top megkülönböztetés
    - `plot("name", price, MAIN|DOT, color)` → fő chartra rajzol
31. **H&S H4 WFO PASS (2026-03-15, CDL_Train.c)**:
    - H4 BarPeriod=240, W=60, hsThresh=0.5 → **WFO PF 1.42**, 101 trade, Win 60.4%
    - Failed H&S fordítás = kulcs: ár visszamegy shoulders fölé → LONG (pattern trap)
    - SL = shoulders szint (pattern érvénytelenítés), TP = fej-neckline távolság
    - 1 trade / detekció (hsDir=0 reset belépés után)
    - H1-en NEM működik (PF 0.92) — H4+ kell chart pattern-hez
    - Default params = optimális (optimizer megerősítette)
    - **Első chart pattern stratégia ami WFO-ban is tartja magát forex-en!**
32. **lite-C "No bars generated" okok (2026-03-15)**:
    - `return;` (érték nélkül) `function`-ban (= `int` típus) → undefined behavior
    - Régi `.par` fájl inkompatibilis param nevekkel → `Data/*.par` törlése kell
    - `strdate(NOW, 0)` crashelhet lite-C-ben
    - `optimize()` nélkül `set(PARAMETERS)` nélkül is okozhat "No bars" ha régi .par interferál

## WORKFLOW SZABÁLYOK
1. Tervezz ELŐTT (3+ lépésnél plan mode)
2. Ha félremegy → ÁLLJ MEG, tervezd újra
3. Soha ne jelölj késznek bizonyítás nélkül
4. Minden user-javítás → lessons.md frissítés
5. Hack helyett elegáns megoldás
6. Bug report → azonnal javítsd, ne kérdezgess
7. **BACKUP MINDIG** nagy refaktor előtt: `.before_xyz` másolat — egyetlen cp visszaállít
