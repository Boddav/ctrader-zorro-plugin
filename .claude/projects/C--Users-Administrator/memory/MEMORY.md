# Claude Memory - cTrader Zorro Projects

## Language & User Preferences
- **Hungarian** communication preferred
- Deploy: PowerShell `Copy-Item -Force` (bash cp fails on locked DLLs)
- Zorro instances: z3, z7.2.7, **z7 3.0** (Zorro 3.01, 2026 feb) (all Desktop/)
- cBot deploy: 3 locations under `Documents/cAlgo/Sources/Robots/`

## Capital.com Plugin (v1.0.0, ÚJ 2026-07-05)
- **Repo**: `C:\Users\Administrator\source\repos\zorro-capitalcom-plugin\` — REST API, build OK
- See [capitalcom_plugin.md](capitalcom_plugin.md) — API kutatás a repo docs/ mappában
- [gold_capitalcom_research.md](gold_capitalcom_research.md) — GOLD EMA_MACD teszt: spread 0.30 megöli, csak H4+ élhet

## EhlersML — Evaluation Shell stratégia (AKTÍV, ÚJ 2026-07-12)
- **z11** (Zorro S 3.01): `Desktop\z11\Strategy\EhlersML.c` — Ehlers DSP (Decycle/CTI/StochEhlers) + PERCEPTRON GO/SKIP + OptimalF
- Shell: `include\eval.c` v1.10; jobok: `Job\EhlersML_ML.csv` / `_NoML.csv`; backtest 2017–2024
- See [ehlersml-z11](ehlersml_z11.md) — shell konvenciók, parser csapdák, pipeline

## Active Project: Zorro-cTrader Bridge (v2.1)
- **Repo**: `C:\Users\Administrator\source\repos\zorro-ctrader-bridge\`
- See [zorro_ctrader_bridge.md](zorro_ctrader_bridge.md) for full details

## WebSocket Plugin (v4.12.0) — Saját/Developer verzió
- **Dir**: `C:\Users\Administrator\source\repos\zorro-plugin-windows-32-4\`
- v4.10.0: orphan order cancel + token race fix | v4.11.0: instance label namespace
- v4.12.0 (2026-07-02): **per-instance margin budget** — `Plugin\cTrader.ini` → `MaxMarginPct = 40`
- **DEPLOY PENDING (user kézzel)** — z9 Plugin = v4.10, z8 = régi; DLL+ini a Release\-ben
- See [websocket_plugin_history.md](websocket_plugin_history.md)

## Zorro Broker Plugin API Reference
- See [zorro_api_research.md](zorro_api_research.md) for full details

## Zorro Training Mode (KRITIKUS)
- `set(RULES)` → advise() PERCEPTRON → _ml.c
- `set(PARAMETERS)` → optimize() → .par
- optimize() MINDIG top-level, SOHA nem if/else-ben!
- Zorro slider limit: max 4 db (0-3)
- optimize() MUST be BEFORE continue
- z3.0: `while(asset(loop(...)))` + `algo()` + `optimize()` = helyes multi-asset minta
- z3.0: BarPeriod=240 + TimeFrame=4 = D1, NEM H4!

## Zorro Hedge & Trade Loop
- `Hedge = 2`, `for(open_trades)`, `for(current_trades)`, `exitTrade(ThisTrade)`

---

## ML-DRIVEN Strategy (AKTÍV, LEGJOBB) — "DIÁK"
- **Szerepe**: MLDRIVEN = **diák** (student), MLDATACOLLECTION = **tanár** (teacher)
- **Fájlok**: `Desktop/z3/Strategy/MLDRIVEN.c` (diák) + `MLDATACOLLECTION.c` (tanár v6)
- **Szerver**: `TENSORFLOWMODEL.py` v6.1 (XGBoost) — port 5001
- **Endpoints**: `/predict`, `/filter`, `/exit_check`, `/retrain`, `/health`
- **Indítás**: `start_ml_xgb.bat` vagy `start_ml_lgbm.bat`
- See [mldriven_strategy.md](mldriven_strategy.md) for full details

### ML Model Összehasonlítás (6 év backtest)
| Model | PF | Trades | CAGR | DD |
|---|---|---|---|---|
| **XGBoost** | **1.59** | 1213 | 13.54% | **2076$** |
| **LightGBM** | 1.45 | 1240 | **21.44%** | 6645$ |
- XGBoost = stabil, legjobb PF → FTMO-ra jobb
- LightGBM = legtöbb profit → agresszívebb

### Filter Accuracy (GO/SKIP)
- SMA: XGBoost **78%** > GBR 76.7%
- CH: GBR **61.4%** > XGBoost 55.2%

### Tanár (MLDATACOLLECTION)
- **v6 (JELENLEGI)**: 12 param, 3 asset — NE tréningezd újra!
- **Backup CSV**: `Strategy/c backup/` — KRITIKUS, NE TÖRÖLD!

---

## MLDRIVEN_CDL Unified (z7 3.0)
- **Fájl**: `Desktop/z7 3.0/Strategy/MLDRIVEN_CDL.c`
- 4 ALGO párhuzamosan: RISK, SMA, CH, HS — nincs FCFS
- **CFG_CAPITAL = 2000** (éles számla méret)
- **Profit védelem**: `RISK_DAILY_PROFIT` define-ból (0=off, 0.02=2%)
- **Slider 0 NEM szabad** — mind a 4 slider (0-3) foglalt
- See [mldriven_cdl_experiment.md](mldriven_cdl_experiment.md) for full details

### CDL Trade Logger + Filter Retrain (2026-03-15, LEZÁRVA)
- Filter retrain: SMA 78→80% | CH 55→60% — javult
- **PF 0.96→0.98** — filter jól szűr (phantom negatív), de alap CH WFO túl gyenge
- **Végkövetkeztetés**: MLDRIVEN + ML szerver (PF 1.59) marad a legjobb

---

## H&S Stratégia (2026-03-15, WFO PASS)
- **Fájl**: `z7 3.0/Strategy/CDL_Train.c`
- **WFO PF 1.42**, Win 60.4%, 101 trade, EUR/USD H4
- Failed H&S fordítás: ár visszamegy shoulders fölé → LONG

## predict()+SMA Stratégia (2026-03-14)
- **PF 1.19**, 1359 trade, 3 asset H1
- predict(CROSSOVER) + SMA 20/50 + ADX>25 + MMI<75 + pyramid 4

---

## BUKOTT Fejlesztések — NE ISMÉTELD!
- See [failed_experiments.md](failed_experiments.md) for full archive
- **Kulcs tanulságok**:
  - ML=irány predikció SOHA nem működött (r<0.08)
  - ML=szűrő (GO/SKIP) és param predikció MŰKÖDIK
  - Max ~40 optimize param WFO-hoz! 3 asset az optimum
  - ICT/classical features NEM PREDIKTÍVEK
  - Több model ≠ jobb (ensemble/hybrid gyengébb)
  - WFO újratréning instabil — backup CSV-k a biztos alap
  - MINDIG ellenőrizd look-ahead-et! `center=True` a leggyakoribb hiba

## lite-C nyelvi korlátok
- See [failed_experiments.md](failed_experiments.md) — alsó szekció
- Ternary NEM létezik, array size literál kell, `function`=`int`
- `#define` értéket NEM lehet `if()`-ben használni → előbb `var`-ba másolni!

---

## Kód Audit (2026-03-07, frissítve 03-10)
- **Current_State eltérés**: tanár `Balance-InitialBalance`, diák `Equity-Capital`
- **CH LifeTime**: tanár optimize(), diák hardcode 0
- See [lessons.md](lessons.md) for workflow rules

## Strategy Search Project (2026-03-10)
- **Dir**: `Desktop/z3/Strategy/itc/` — Python ML pipeline
- **Legjobb**: XAUUSD M15 EMA_MACD+RR2.0 — PF=1.52, 5/8 WFO
- See [ict_project.md](ict_project.md) for full details

## Egyéb Stratégiák
- **HedgingClaude v11**: PF 2.03, SR 1.01, CAGR 24.91%
- **MartingaleHedge v3**: PF 3.89, Win 57.1%
- See [hedgingclaude_optimization.md](hedgingclaude_optimization.md)

## Vapi AI Hideghívó Platform (2026-06-09)
- **FreePBX 17**: `46.29.140.190`, ePhone SIP: `+36 21 2000 479`
- **Vapi Assistant**: `ab55b35b` (Dávid), SIP: `ephonebridge1@sip.vapi.ai`
- **Permanent fix**: `pjsip.endpoint_custom_post.conf` → `[Vapi](+)\noutbound_proxy=`
- **404 hiba oka**: `outbound_proxy=sip:sip.vapi.ai` felülírta Request-URI-t (username elveszett)
- See [vapi_freepbx.md](vapi_freepbx.md) for full details

## Server & References
- **DAVID**: Windows Server 2022, Rackhost VPS, 16GB RAM
- [ftmo_rules.md](ftmo_rules.md) | [ctrader_api_reference.md](ctrader_api_reference.md)
- [feedback_no_easy_way.md](feedback_no_easy_way.md) — Ne menekülj egyszerű megoldásba
- [feedback_no_credentials.md](feedback_no_credentials.md) — Hitelesítési adatokat SOHA ne írj ki/hardcode-olj
