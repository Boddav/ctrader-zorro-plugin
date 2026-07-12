// =================================================================
// ML DATA COLLECTION v6 - javított verzió (2025-03)
// SMA + CH algók, FCFS, TimeFrame támogatás, phantom label-ek
// Javítások: SMA timeframe Close, CH RegLine target exit
// =================================================================

#define NUM_ASSETS 3
#define MAX_LAYERS 4
#define MAX_PHANTOM 20

static var InitialBalance;
static int lastLogDay[NUM_ASSETS];
static int csvReady;
static int layersLong[NUM_ASSETS];
static int layersShort[NUM_ASSETS];

// FCFS tracking per asset
static int smaHasOpen[NUM_ASSETS];
static int chHasOpen[NUM_ASSETS];

// Phantom trade tracking
static var phEntry[MAX_PHANTOM];
static int phDir[MAX_PHANTOM];
static int phBars[MAX_PHANTOM];
static int phAsset[MAX_PHANTOM];
static int phType[MAX_PHANTOM];       // 0 = SMA, 1 = CH
static var phATR[MAX_PHANTOM];
static var phStop[MAX_PHANTOM];
static var phFeats[240];

// Segédfüggvény a phantom rekordoláshoz (kód rövidítése)
function phantom_record(int aIdx, int dir, int type, var atr, var stopDist, var* feats)
{
	int slot = -1;
	int s;
	for(s = 0; s < MAX_PHANTOM; s++)
		if(phDir[s] == 0) { slot = s; break; }
	if(slot < 0) return;

	phEntry[slot] = priceClose();
	phDir[slot]   = dir;
	phBars[slot]  = 0;
	phAsset[slot] = aIdx;
	phType[slot]  = type;
	phATR[slot]   = atr;
	phStop[slot]  = stopDist;

	int base = slot * 12;
	int f;
	for(f = 0; f < 12; f++)
		phFeats[base + f] = feats[f];
}

function run()
{
    BarPeriod = 60;
    LookBack = 900;
    StartDate = 20200102;
    EndDate = 20261018;

    set(LOGFILE|PRELOAD|PARAMETERS|FACTORS|RULES);
    Capital = 10000;
    Leverage = 500;
    Hedge = 2;
    StopFactor = 1.5;
    EndWeek = 52200;

    NumWFOCycles = 10;
    DataSplit = 80;

    if(is(INITRUN))
    {
        InitialBalance = Balance;
        csvReady = 0;
        int k;
        for(k = 0; k < NUM_ASSETS; k++)
        {
			
			
            lastLogDay[k] = 0;
            layersLong[k] = 0;
            layersShort[k] = 0;
            smaHasOpen[k] = 0;
            chHasOpen[k] = 0;
        }
        for(k = 0; k < MAX_PHANTOM; k++)
            phDir[k] = 0;

        string header1 = "Date,Asset,ATR_Pct,Range_Pct,Volatility,ADX,Trend_Bias,Trend_Quality,RSI,Hurst,Return_20,BB_Width,WinRate,Current_State,smaTF,FastMA,SlowMA,smaStop_x10,adxSMA,mmiSMA,N,Factor_x100,chStop_x10,lifeTime,adxCH,mmiCH\n";
        file_write("MLTrainingData.csv", header1, 0);

        string header2 = "Asset,EntryType,Direction,ATR_Pct,Range_Pct,Volatility,ADX,Trend_Bias,Trend_Quality,RSI,Hurst,Return_20,BB_Width,WinRate,Current_State,PnlPips,Result\n";
        file_write("MLTradeData.csv", header2, 0);

        csvReady = 1;
    }

    // === MULTI-ASSET LOOP ===
    while(asset(loop("EUR/USD", "GBP/USD", "USD/JPY")))
    {
        int aIdx = 0;
        string assetCode = "EURUSD";
        if(strstr(Asset, "EUR/USD"))      { aIdx = 0; assetCode = "EURUSD"; }
        else if(strstr(Asset, "GBP/USD")) { aIdx = 1; assetCode = "GBPUSD"; }
        else if(strstr(Asset, "USD/JPY")) { aIdx = 2; assetCode = "USDJPY"; }

        // =========================================
        // SMA ALGO PARAMS (6 optimize)
        // =========================================
        algo("SMA");
        int smaTF = optimize(4, 1, 8, 1); if(smaTF < 1) smaTF = 1;
        int FastMA = optimize(20, 10, 40, 5);
        int SlowMA = optimize(50, 40, 100, 10);
        int smaStop_x10 = optimize(30, 15, 50, 5);
        var smaStop = smaStop_x10 / 10.0;
        int adxSMA = optimize(25, 15, 40, 5);
        int mmiSMA = optimize(75, 60, 85, 5);
        if(FastMA >= SlowMA) SlowMA = FastMA + 10;

        // =========================================
        // CH ALGO PARAMS (6 optimize)
        // =========================================
        algo("CH");
        int N = optimize(60, 30, 120, 10); if(N < 10) N = 10;
        int Factor_x100 = optimize(20, 10, 40, 5);
        var Factor = Factor_x100 / 100.0;
        int chStop_x10 = optimize(30, 15, 50, 5);
        var chStop = chStop_x10 / 10.0;
        int lifeTime = optimize(20, 10, 40, 5);
        int adxCH = optimize(25, 15, 40, 5);
        int mmiCH = optimize(75, 60, 85, 5);

        // =========================================
        // SHARED INDICATORS
        // =========================================
        TimeFrame = 1;
        vars Close = series(priceClose());
        var price = Close[0];

        var rsi = RSI(Close, 14);
        if(rsi != rsi) rsi = 50;
        var adx = ADX(14);
        if(adx != adx) adx = 25;

        TimeFrame = 4;
        vars H4Close = series(priceClose());
        var h4atr = ATR(14);
        TimeFrame = 1;

        var hurst = Hurst(Close, 100);
        if(hurst != hurst) hurst = 0.5;

        vars MMI_Raw = series(MMI(Close, 200));
        vars MMI_Smooth = series(SMA(MMI_Raw, 50));
        var mmiVal = MMI_Smooth[0];
        if(mmiVal != mmiVal) mmiVal = 75;

        vars SMA20 = series(SMA(Close, 20));
        vars SMA50 = series(SMA(Close, 50));
        vars BarIdx = series(Bar);

        // SMA indicators at smaTF timeframe ─────── JAVÍTOTT ───────
        TimeFrame = smaTF;
        vars CloseTF = series(priceClose());
        vars SMA_F   = series(SMA(CloseTF, FastMA));
        vars SMA_S   = series(SMA(CloseTF, SlowMA));
        TimeFrame = 1;

        // =========================================
        // LINREG CHANNEL
        // =========================================
        var Slope = LinearRegSlope(Close, N);
        var Intercept = LinearRegIntercept(Close, N);
        if(Slope != Slope) Slope = 0;
        if(Intercept != Intercept) Intercept = Close[0];

        var HighDev = 0, LowDev = 0;
        int i;
        for(i = N; i > 0; i--)
        {
            var LinVal = Intercept + Slope * (N - i);
            HighDev = max(HighDev, Close[i] - LinVal);
            LowDev = min(LowDev, Close[i] - LinVal);
        }

        var RegLine = Intercept + Slope * N;
        var ChannelWidth = HighDev - LowDev;
        var EntryLow  = RegLine + LowDev  + Factor * (HighDev + LowDev);
        var EntryHigh = RegLine + HighDev - Factor * (HighDev + LowDev);

        // Session filter
        int hr = hour();
        int isRollover = (hr >= 21 && hr <= 22);
        int sessionOK = 0;
        if(strstr(Asset, "EUR/USD"))      sessionOK = (hr >= 7 && hr <= 20);
        else if(strstr(Asset, "GBP/USD")) sessionOK = (hr >= 7 && hr <= 20);
        else if(strstr(Asset, "USD/JPY")) sessionOK = (hr >= 1 && hr <= 16);
        else sessionOK = 1;
        if(isRollover) sessionOK = 0;

        int skipBar = (price == 0 || h4atr < PIP * 5 || ChannelWidth < PIP * 3 || !sessionOK);

        // =========================================
        // MARKET FEATURES (12)
        // =========================================
        var ATR_Pct = 0;
        if(price > 0) ATR_Pct = h4atr / price * 100;
        var Range_Pct = 0;
        if(price > 0) Range_Pct = (priceHigh() - priceLow()) / price * 100;
        var Volatility = StdDev(Close, 20);
        if(Volatility != Volatility) Volatility = 0;
        var Trend_Bias = 0;
        if(SMA50[0] > 0) Trend_Bias = (SMA20[0] - SMA50[0]) / SMA50[0] * 100;
        var Trend_Quality = Correlation(Close, BarIdx, 20);
        if(Trend_Quality != Trend_Quality) Trend_Quality = 0;
        var Return_20 = 0;
        if(Close[20] > 0) Return_20 = (Close[0] - Close[20]) / Close[20] * 100;
        var BB_Upper = BBands(Close, 20, 2, 2, MAType_SMA);
        var BB_Middle = rRealMiddleBand;
        var BB_Lower = rRealLowerBand;
        var BB_Width = 0;
        if(BB_Middle > 0) BB_Width = (BB_Upper - BB_Lower) / BB_Middle * 100;
        var totalTrades = NumWinTotal + NumLossTotal;
        var WinRate = 0;
        if(totalTrades > 0) WinRate = (var)NumWinTotal / totalTrades;
        var Current_State = 0;
        if(Capital > 0) Current_State = (Balance - InitialBalance) / Capital * 100;

        var feats[12];
        feats[0] = ATR_Pct;   feats[1] = Range_Pct;
        feats[2] = Volatility; feats[3] = adx;
        feats[4] = Trend_Bias; feats[5] = Trend_Quality;
        feats[6] = rsi;        feats[7] = hurst;
        feats[8] = Return_20;  feats[9] = BB_Width;
        feats[10] = WinRate;   feats[11] = Current_State;

        // =========================================
        // PHANTOM TRADE EXIT CHECK
        // =========================================
        if(!is(TRAINMODE) && csvReady)
        {
            int p;
            for(p = 0; p < MAX_PHANTOM; p++)
            {
                if(phDir[p] == 0) continue;
                if(phAsset[p] != aIdx) continue;

                phBars[p]++;
                var phPnl = (price - phEntry[p]) * phDir[p];

                int doExit = 0;
                if(phPnl < -(phATR[p] * phStop[p])) doExit = 1;

                int sessionEndPhantom = 0;
                if(aIdx == 0 || aIdx == 1) sessionEndPhantom = (hr >= 20);
                if(aIdx == 2) sessionEndPhantom = (hr >= 16);

                if(phType[p] == 0) // SMA
                {
                    int smaXL = crossOver(SMA_F, SMA_S);
                    int smaXS = crossUnder(SMA_F, SMA_S);
                    if(phDir[p] == 1 && smaXS) doExit = 1;
                    if(phDir[p] == -1 && smaXL) doExit = 1;
                }
                else // CH
                {
                    if(phDir[p] == 1 && price > EntryHigh) doExit = 1;
                    if(phDir[p] == -1 && price < EntryLow) doExit = 1;
                    if(phBars[p] >= lifeTime) doExit = 1;
                    if(sessionEndPhantom) doExit = 1;
                }

                if(phBars[p] >= 80) doExit = 1;

                if(doExit)
                {
                    var costPips = max(Spread, 5);
                    var netPnl = phPnl / PIP - costPips;
                    int result = 0;
                    if(netPnl > 0) result = 1;

                    int base = p * 12;
                    string line = strf("%s,%s,%d,%.4f,%.4f,%.6f,%.2f,%.4f,%.4f,%.2f,%.4f,%.4f,%.4f,%.4f,%.2f,%.1f,%d\n",
                        assetCode, ifelse(phType[p]==0,"SMA","CH"), phDir[p],
                        phFeats[base+0], phFeats[base+1], phFeats[base+2],
                        phFeats[base+3], phFeats[base+4], phFeats[base+5],
                        phFeats[base+6], phFeats[base+7], phFeats[base+8],
                        phFeats[base+9], phFeats[base+10], phFeats[base+11],
                        netPnl, result);

                    file_append("MLTradeData.csv", line);
                    phDir[p] = 0;
                }
            }
        }

        if(skipBar) continue;

        // =========================================
        // NAPI LOG - param + feature CSV
        // =========================================
        if(!is(TRAINMODE) && Bar > LookBack && csvReady)
        {
            int today = day(0);
            if(today != lastLogDay[aIdx])
            {
                lastLogDay[aIdx] = today;
                char dateBuf[16];
                sprintf(dateBuf, "%04d-%02d-%02d", year(0), month(0), day(0));

                string logLine = strf("%s,%s,%.4f,%.4f,%.6f,%.2f,%.4f,%.4f,%.2f,%.4f,%.4f,%.4f,%.4f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                    dateBuf, Asset,
                    ATR_Pct, Range_Pct, Volatility, adx, Trend_Bias,
                    Trend_Quality, rsi, hurst, Return_20, BB_Width,
                    WinRate, Current_State,
                    smaTF, FastMA, SlowMA, smaStop_x10, adxSMA, mmiSMA,
                    N, Factor_x100, chStop_x10, lifeTime, adxCH, mmiCH);

                file_append("MLTrainingData.csv", logLine);
            }
        }

        // =========================================
        // FCFS CHECK (csak Test módban)
        // =========================================
        algo("SMA");   smaHasOpen[aIdx] = (NumOpenLong + NumOpenShort > 0);
        algo("CH");    chHasOpen[aIdx] = (NumOpenLong + NumOpenShort > 0);

        int smaCanOpen = 1;
        int chCanOpen = 1;
        if(!is(TRAINMODE))
        {
            smaCanOpen = !chHasOpen[aIdx];
            chCanOpen = !smaHasOpen[aIdx];
        }

        // =========================================
        // SIGNALS
        // =========================================
        int smaRegime = (adx > adxSMA && mmiVal < mmiSMA);
        int smaLongSig  = crossOver(SMA_F, SMA_S);
        int smaShortSig = crossUnder(SMA_F, SMA_S);
        int smaOK_L = smaLongSig  && rsi > 45 && rsi < 70 && smaRegime && smaCanOpen;
        int smaOK_S = smaShortSig && rsi < 55 && rsi > 30 && smaRegime && smaCanOpen;

        int chRegime = (adx > adxCH && mmiVal < mmiCH);
        int chLong   = price < EntryLow;
        int chShort  = price > EntryHigh;
        int chOK_L = chLong  && chRegime && chCanOpen;
        int chOK_S = chShort && chRegime && chCanOpen;

        // =========================================
        // SMA EXIT: opposite crossover
        // =========================================
        algo("SMA");
        if(smaShortSig && NumOpenLong > 0)  { exitLong();  layersLong[aIdx] = 0; }
        if(smaLongSig  && NumOpenShort > 0) { exitShort(); layersShort[aIdx] = 0; }

        // =========================================
        // CH EXIT: session end + RegLine target ─────── JAVÍTOTT ───────
        // =========================================
        algo("CH");
        int sessionEnd = 0;
        if(aIdx == 0 || aIdx == 1) sessionEnd = (hr >= 20);
        if(aIdx == 2)              sessionEnd = (hr >= 16);

        if(sessionEnd)
        {
            exitLong();
            exitShort();
        }
        else
        {
            if(NumOpenLong  > 0 && price > EntryHigh) exitLong();
            if(NumOpenShort > 0 && price < EntryLow)  exitShort();
        }

        // =========================================
        // SMA ENTRY (pyramid)
        // =========================================
        algo("SMA");
        if(NumOpenLong == 0) layersLong[aIdx] = 0;
        if(NumOpenShort == 0) layersShort[aIdx] = 0;

        if(smaOK_L && layersLong[aIdx] < MAX_LAYERS)
        {
            Stop = h4atr * smaStop;
            Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
            LifeTime = 0;
            enterLong();
            layersLong[aIdx]++;

            if(!is(TRAINMODE) && csvReady) phantom_record(aIdx, 1, 0, h4atr, smaStop, feats);
        }

        if(smaOK_S && layersShort[aIdx] < MAX_LAYERS)
        {
            Stop = h4atr * smaStop;
            Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
            LifeTime = 0;
            enterShort();
            layersShort[aIdx]++;

            if(!is(TRAINMODE) && csvReady) phantom_record(aIdx, -1, 0, h4atr, smaStop, feats);
        }

        // =========================================
        // CH ENTRY (max 2 layer, no overnight)
        // =========================================
        algo("CH");
        int maxCHLayers = 2;   // később akár optimize-olható

        if(chOK_L && NumOpenLong < maxCHLayers && !sessionEnd)
        {
            Stop = h4atr * chStop;
            Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
            LifeTime = lifeTime;
            enterLong();

            if(!is(TRAINMODE) && csvReady) phantom_record(aIdx, 1, 1, h4atr, chStop, feats);
        }

        if(chOK_S && NumOpenShort < maxCHLayers && !sessionEnd)
        {
            Stop = h4atr * chStop;
            Margin = Equity * 0.5 / 100.0 / NUM_ASSETS;
            LifeTime = lifeTime;
            enterShort();

            if(!is(TRAINMODE) && csvReady) phantom_record(aIdx, -1, 1, h4atr, chStop, feats);
        }
    } // end while(asset)

    if(is(EXITRUN))
    {
        printf("\nML DATA COLLECTION v6 - javított verzió");
        printf("\nSMA: timeframe javítva, CH: RegLine target exit hozzáadva");
        printf("\nKét CSV: MLTrainingData.csv + MLTradeData.csv");
    }
}
