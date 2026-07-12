---
name: ehlersml-z11
description: "EhlersML stratégia a z11-ben — Ehlers DSP + Perceptron GO/SKIP + OptimalF, Zorro Evaluation Shell-lel"
metadata: 
  node_type: memory
  type: project
  originSessionId: 062a6d17-fae3-4e6e-8390-3bef110292ac
---

# EhlersML — Evaluation Shell stratégia (z11, 2026-07-12)

- **Fájl**: `Desktop\z11\Strategy\EhlersML.c` — lite-C, Workshop6c minta
- **Zorro**: z11 = Zorro S 3.01 (build 16862), Zorro S token a Zorro.ini-ben
- **Shell**: `include\eval.c` v1.10 + `evars.h`; a user `run()`-ját az eval.h `strategy()`-vé nevezi át
- **Algók**: TRND (Decycle + CTI gate, dip-buy) és CYCL (StochEhlers ellentrend, Roof szűrővel)
- **ML**: `adviseLong/Short(PERCEPTRON)` GO/SKIP szűrő 6 normalizált regime feature-rel
  (MMI, CTI, FisherN, HTDcPeriod, ATR30/ATR200, HTTrendMode); `MLThreshold=-100` = szűrő ki
- **OptimalF**: `Margin = 0.5*OptimalF*abs(Capital)`, Capital=-10000 reinvest, set(FACTORS)
- **Fix a szkriptben** (panel nem írja felül): StartDate=2017, EndDate=20241231, BarPeriod=60, LookBack=2500
- **Variánsok** (2026-07-12 bővítve): `_ASSETS` EUR/USD, USD/JPY, USD/CHF, AUD/USD, USD/CAD (GBP/USD kizárva — vesztes) × `_ALGOS` TRND,CYCL × `_TIMEFRAMES` 60,240
- **Session + Risk réteg**: SessionStart/End (UTC belépési ablak), BarShift (H4 FrameOffset), RiskDayLoss (napi halt %), RiskMaxDD (equity DD halt %) — riskGuard() flatten+halt, EntryOK kapuzza a belépéseket
- **Eredmények (2026-07-12)**: USD/JPY TRND H4 = mag. 6/6 WFO konfig profit (PF 3.0–9.99, Sharpe>1, R2 0.73–0.92), MRC PASS (eredeti +20725$ > mind a 20 kevert futás, p<5%). Kevés trade (~5/év) + jen-trend koncentráció = fő kockázat
- **SESSION-SZŰRŐ ÁTTÖRÉS**: SessionStart/End WFO-optimalizálva (csak _optimize()-zal működik!) — USDJPY TRND H4: PF 3.94→9.90, DD 20%→6%! | USDJPY CYCL H4: 1.21→1.62 | USDCAD CYCL H4: 1.10→1.33 | DE: AUD CYCL H4-nél és JPY TRND H1-nél RONTOTT → ott ki. Minden H1 CYCL veszteséges (spread), H4 a működő idősík
- **VÉGSŐ PORTFÓLIÓ (VALIDÁLVA 2026-07-12)**: `EhlersPortfolio.c` — USD/JPY TRND + USD/JPY CYCL + AUD/USD CYCL (session nélkül!) + USD/CAD CYCL (OptimalF 0-ra súlyozta), mind H4; standalone, asset/algo loop, OptimalF, riskGuard beépítve (RiskDayLoss/RiskMaxDD globálok, 0=off)
- **Portfólió WFO eredmény**: PF 1.70, PRR 1.46, 341 trade, Win 56%, AR 55%, Sharpe 1.39, MaxDD 13.4%, R2 0.615 (2021.07–2024.12 OOS). Évek: 2021 +37%, 2022 +172%(!), 2023 +34%, 2024 −7% — a 2022-es jen-trend dominál
- **Portfólió MRC PASS**: eredeti +36169$ (PF 1.70) vs 20 kevert futás (átlag −2800$, max +14326$) — 0/20 verte, z≈3.9
- Session ablakok (WFO átlag): JPY TRND 5→18 UTC, JPY CYCL 5→15 UTC — Tokió-vég/London/NY, gazdaságilag értelmes; paraméterek a tartomány közepén (nincs határérték-tapadás)
- MRC mód a portfólió szkriptbe építve: `Data\pfmrc.txt` (ciklusszám) megléte kapcsolja; eredmények Log\pfmrc_results.csv
- Élesítéshez: RiskDayLoss=4, RiskMaxDD=10 (FTMO), min. tőke ~1500$ vagy cent-számla (H4 stop 400-600 pip × 0.01 lot ≈ 26-40$ kockázat/trade)
- EhlersCheck.c (standalone WFO check, config Data\checkcfg.txt-ből) + EhlersMRC.c (Detrend=SHUFFLE Montecarlo) — headless batch minta a scratchpad runcheck3.ps1/runmrc.ps1-ben; Zorro CLI: -run/-train (NEM -test!), -quiet; a -d NEM #define, csak Define string!
- **Jobok**: `Job\EhlersML_ML.csv` (szűrő be) és `Job\EhlersML_NoML.csv` (baseline) — formátum: 1. sor a szkript neve, majd `név,érték,min,max,step`; step≠0 = optimalizált; a betöltés NÉV szerint történik, csak a szkript-változókat tölti (Type&8)
- **History átmásolva** z7 3.0-ból: GBPUSD 2017–2025, USDJPY 2019–2025, XAUUSD 2019–2025 (2025 mind hiányos → ezért EndDate 2024)

## Shell tudnivalók (eval.c v1.10)
- Panel változók a .c fájl elejéről parsolódnak: `var NAME;<tab>//= default, min..max, step; leírás` — a parser az első `sizeof(V)` db sor-eleji `var `-t olvassa, EZUTÁN jöhet csak más sor-eleji `var` (pl. függvény)!
- `_optimize(V.x)` (=optv) csak akkor optimalizál, ha a step≠0 — feltételes hívás megengedett
- `NumWFOCycles`/`DataSplit` a szkriptben NO-OP (eval.h átdefiniálja) — a WFO-t a panel `_WFO_Cycles/_WFO_OOS` vezérli
- `_Investment=3` = beépített OptimalF mód slider-rel (mi a W6c-féle Margin sort használjuk helyette)
- Pipeline: Load Jobs from Folder → [Train] → Summary → Cluster (Job\WFO_Cluster3x3.csv) → CA%≥60+PF≥1.25 → portfólió → Montecarlo (p<5%)
- **KRITIKUS: kombinált RULES+PARAMETERS tréning NEM MEGY a shellben** (manual: "only works with single assets, not with a portfolio system that contains asset or algo calls"). Tünet: minden optimize-lépés AZONOS objective+trade számot ad (a paraméter nem jut el a szimulációba), a kiválasztott érték értelmetlen
- **Megoldás: kétmenetes MLMode architektúra** (2026-07-12):
  - MLMode=0 job (step≠0): tiszta PARAMETERS+FACTORS WFO tréning, advise/RULES teljesen ki — ezen fut a teljes shell pipeline
  - MLMode=1 job (MINDEN step=0, értékek az 1. menet nyerteseiből): optv nem hív optimize()-t → csak set(RULES) perceptron tréning fut a fix rendszeren; Test-ben a szűrő él
  - Entry: `Go=100; if(V.MLMode>0) Go=mlGoLong(); if(Train || Go>V.MLThreshold) enterLong();`
- advise csapda (részlete): param-fázisban advise()=0 → advise-kapuzott entry 0 trade-del optimalizál; tréningben SOHA ne kapuzz
- Másik tanulság: 2 core-os WFO tréningnél a 2. core semmit nem írt (páros ciklusú .par-ok hiányoztak → Test-ben Error 062/044) → `_Max_Threads=1` a PANELEN (a job CSV rendszer-változóit a loadVarsCSV NEM tölti be, csak a stratégia-változókat!); ok tisztázatlan (~12 párhuzamos Zorro instance fut a gépen)
- **NumCores-t (és más rendszer-beállítást, amit a panel kezel) TILOS a stratégiában hardcode-olni** — a strategy() az evalSet UTÁN fut, felülírja a panelt (2026-07-12: a hardcoded NumCores=-2 emiatt győzte le a panel _Max_Threads=1-et)
- Diag mód (`-diag`) nagyon lassú compile-t okoz; diag.txt-t zárolja
- Ablak-ellenőrzés RDP-n: PrintWindow API-s PowerShell szkriptek a scratchpadban (capwin.ps1/caphwnd.ps1 minta)
