// EhlersPortfolioFix: LIVE version - fixed parameters, no ML, riskGuard
// validated 2026-07-12: PF 1.63, Sharpe 1.62, MaxDD 11.6%, R2 0.77 (OOS)
// components (all H4): USD/JPY TRND (session 5-18 UTC),
//   USD/JPY CYCL (session 5-15), AUD/USD CYCL (no session)
// params = 10-cycle WFO averages, no retraining needed

var RiskDayLoss = 0;	// live: 4 = halt at -4% equity in a day, 0 = off
var RiskMaxDD = 0;	// live: 10 = halt at -10% from equity peak, 0 = off
var MarginPct = 3;	// margin per component in % of capital

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
		RiskHalt = 2;	// stays halted, manual restart required
	if(RiskHalt) {	// flatten current component, no new entries
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

void tradeTrend()	// USD/JPY: dip buy/rally sell in CTI-confirmed trend
{
	vars Prices = series(price(0));
	vars Trends = series(Decycle(Prices,129));
	vars CTIs = series(CTI(Prices,60));
	Stop = 14.5 * ATR(100);
	Trail = 0;

	int Ok = EntryOK && sessionOK(5,18);
	if(Ok && CTIs[0] > 0.43 && valley(Trends))
		enterLong();
	else if(Ok && CTIs[0] < -0.43 && peak(Trends))
		enterShort();
}

void tradeCycle()	// StochEhlers counter-trend at the band edges
{
	var S1 = 0, S2 = 24, Period = 30, StopD = 15.6;
	if(!strstr(Asset,"AUD")) {	// USD/JPY settings
		S1 = 5; S2 = 15; Period = 27; StopD = 10.6;
	}
	vars Prices = series(price(0));
	vars Stochs = series(StochEhlers(Prices,Period,10,48));
	Stop = StopD * ATR(100);
	Trail = 4 * ATR(100);

	int Ok = EntryOK && sessionOK(S1,S2);
	if(Ok && crossUnder(Stochs,0.21))
		enterLong();
	else if(Ok && crossOver(Stochs,0.79))
		enterShort();
}

function run()
{
	set(LOGFILE);
	BarPeriod = 60;
	LookBack = 2500;
	StartDate = 2019;	// backtest period; ignored in Trade mode
	EndDate = 20241231;
	Capital = -10000;

	while(asset(loop("USD/JPY","AUD/USD")))
	while(algo(loop("TRND","CYCL")))
	{
		TimeFrame = 4;
		MaxLong = MaxShort = 1;
		Margin = 0.01 * MarginPct * abs(Capital);
		riskGuard();
		if(strstr(Algo,"TRND")) {
			if(strstr(Asset,"USD/JPY"))	// TRND only on USD/JPY
				tradeTrend();
		} else
			tradeCycle();
	}
	return 0;
}
