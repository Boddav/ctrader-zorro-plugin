// EhlersPortfolio: final Ehlers DSP portfolio, all H4 components
// USD/JPY TRND + USD/JPY CYCL + AUD/USD CYCL + USD/CAD CYCL
// session windows WFO-optimized per component (off for AUD - hurt OOS)
// riskGuard: daily loss + max drawdown halt, 0 = off for backtests

var RiskDayLoss = 0;	// daily loss halt in % of equity
var RiskMaxDD = 0;	// equity drawdown halt in % from peak

int EntryOK = 1, RiskHalt = 0, RiskDay = 0;
var PeakEquity = 0, DayStartEquity = 0;

void riskGuard()
{
	var Eq = Equity;
	if(is(INITRUN)) {
		PeakEquity = DayStartEquity = Eq;
		RiskHalt = 0;
		RiskDay = 0;
	}
	if(day(0) != RiskDay) {	// new trading day
		RiskDay = day(0);
		DayStartEquity = Eq;
		if(RiskHalt == 1) RiskHalt = 0;	// daily halt ends with the day
	}
	if(Eq > PeakEquity) PeakEquity = Eq;
	if(RiskDayLoss > 0 && Eq < DayStartEquity - 0.01*RiskDayLoss*abs(DayStartEquity))
		RiskHalt = 1;
	if(RiskMaxDD > 0 && Eq < PeakEquity - 0.01*RiskMaxDD*abs(PeakEquity))
		RiskHalt = 2;	// stays halted
	if(RiskHalt) {	// flatten current component
		exitLong("*");
		exitShort("*");
	}
	EntryOK = !RiskHalt;
}

int sessionOK(var S1, var S2)
{
	int H = hour(0);
	if(S1 <= S2) {
		if(H >= S1 && H < S2) return 1;
		return 0;
	}
	if(H >= S1 || H < S2) return 1;	// window crosses midnight
	return 0;
}

void tradeTrend()
{
	var S1 = optimize("SessStart",0,0,21,3);
	var S2 = optimize("SessEnd",24,3,24,3);
	vars Prices = series(price(0));
	vars Trends = series(Decycle(Prices,optimize("Period",100,40,200,20)));
	vars CTIs = series(CTI(Prices,60));
	var Threshold = optimize("CTIThresh",0.4,0.2,0.8,0.1);
	Stop = optimize("StopDist",8,4,20,2) * ATR(100);
	Trail = 0;

	int Ok = EntryOK && sessionOK(S1,S2);
	if(Ok && CTIs[0] > Threshold && valley(Trends))
		enterLong();
	else if(Ok && CTIs[0] < -Threshold && peak(Trends))
		enterShort();
}

void tradeCycle()
{
	var S1 = 0, S2 = 24;
	if(!strstr(Asset,"AUD")) {	// session filter hurt AUD/USD OOS
		S1 = optimize("SessStart",0,0,21,3);
		S2 = optimize("SessEnd",24,3,24,3);
	}
	vars Prices = series(price(0));
	vars Stochs = series(StochEhlers(Prices,optimize("Period",20,10,40,5),10,48));
	var Edge = optimize("Edge",0.2,0.1,0.35,0.05);
	Stop = optimize("StopDist",8,4,20,2) * ATR(100);
	Trail = 4 * ATR(100);

	int Ok = EntryOK && sessionOK(S1,S2);
	if(Ok && crossUnder(Stochs,Edge))
		enterLong();
	else if(Ok && crossOver(Stochs,1.-Edge))
		enterShort();
}

int MRCRuns = 0;	// >1 = Montecarlo mode, from Data\pfmrc.txt

function run()
{
	set(PARAMETERS,FACTORS,LOGFILE);
	BarPeriod = 60;
	LookBack = 2500;
	StartDate = 2017;
	EndDate = 20241231;
	NumWFOCycles = 10;
	DataSplit = 85;
	Capital = -10000;	// reinvestment mode

// Montecarlo Reality Check: cycle 1 = original, 2..N = shuffled prices
	if(is(INITRUN)) {
		MRCRuns = 0;
		string Cfg = file_content("Data\\pfmrc.txt");
		if(Cfg) MRCRuns = atoi(Cfg);
	}
	if(!Train && MRCRuns > 1) {
		NumTotalCycles = MRCRuns;
		set(PRELOAD);
		if(TotalCycle > 1)
			setf(Detrend,SHUFFLE);
		else
			resf(Detrend,SHUFFLE);
	}

	while(asset(loop("USD/JPY","AUD/USD","USD/CAD")))
	while(algo(loop("TRND","CYCL")))
	{
		TimeFrame = 4;
		MaxLong = MaxShort = 1;
		Margin = 0.5 * OptimalF * abs(Capital);
		riskGuard();
		if(strstr(Algo,"TRND")) {
			if(strstr(Asset,"USD/JPY"))	// TRND only on USD/JPY
				tradeTrend();
		} else
			tradeCycle();
	}

	if(is(EXITRUN) && !Train && MRCRuns > 1) {
		var PF = 9.99;
		if(LossTotal > 0) PF = WinTotal/LossTotal;
		file_append("Log\\pfmrc_results.csv",
			strf("%i,%.0f,%.2f,%i\n",TotalCycle,WinTotal-LossTotal,PF,NumWinTotal+NumLossTotal),0);
	}
	return 0;
}
