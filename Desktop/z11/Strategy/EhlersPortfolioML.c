// EhlersPortfolioML: portfolio with FIXED params + perceptron GO/SKIP layer
// params = 10-cycle WFO averages from EhlersPortfolio training
// RULES-only training (combined RULES+PARAMETERS is broken for portfolios)
// MLThreshold read from Data\mlcfg.txt: -100 = filter off (baseline A/B)

var MLThreshold = 0;

// normalized regime features for the perceptron
var FeedMMI,FeedCTI,FeedFisher,FeedDC,FeedVol,FeedTrend;

void mlFeatures()
{
	vars Prices = series(price(0));
	FeedMMI = MMI(Prices,300)/100. - 0.5;
	FeedCTI = CTI(Prices,60);
	FeedFisher = FisherN(Prices,100);
	FeedDC = HTDcPeriod(Prices)/25. - 1.;
	FeedVol = ATR(30)/ATR(200) - 1.;
	FeedTrend = HTTrendMode(Prices) - 0.5;
}

var mlGoLong()
{
	return adviseLong(PERCEPTRON,0,
		FeedMMI,FeedCTI,FeedFisher,FeedDC,FeedVol,FeedTrend);
}

var mlGoShort()
{
	return adviseShort(PERCEPTRON,0,
		FeedMMI,FeedCTI,FeedFisher,FeedDC,FeedVol,FeedTrend);
}

int sessionOK(var S1, var S2)
{
	int H = hour(0);
	if(S1 <= S2) {
		if(H >= S1 && H < S2) return 1;
		return 0;
	}
	if(H >= S1 || H < S2) return 1;
	return 0;
}

void tradeTrend()	// USD/JPY, fixed: Sess 5-18, Period 129, CTI 0.43, Stop 14.5
{
	vars Prices = series(price(0));
	vars Trends = series(Decycle(Prices,129));
	vars CTIs = series(CTI(Prices,60));
	Stop = 14.5 * ATR(100);
	Trail = 0;

	var Go = 100;
	int Ok = sessionOK(5,18);
	if(Ok && CTIs[0] > 0.43 && valley(Trends)) {
		Go = mlGoLong();
		if(Train || Go > MLThreshold)
			enterLong();
	} else if(Ok && CTIs[0] < -0.43 && peak(Trends)) {
		Go = mlGoShort();
		if(Train || Go > MLThreshold)
			enterShort();
	}
}

void tradeCycle()	// USD/JPY: Sess 5-15, P27, Edge .21, Stop 10.6; AUD: no session, P30, Stop 15.6
{
	var S1 = 0, S2 = 24, Period = 30, StopD = 15.6;
	if(!strstr(Asset,"AUD")) {
		S1 = 5; S2 = 15; Period = 27; StopD = 10.6;
	}
	vars Prices = series(price(0));
	vars Stochs = series(StochEhlers(Prices,Period,10,48));
	Stop = StopD * ATR(100);
	Trail = 4 * ATR(100);

	var Go = 100;
	int Ok = sessionOK(S1,S2);
	if(Ok && crossUnder(Stochs,0.21)) {
		Go = mlGoLong();
		if(Train || Go > MLThreshold)
			enterLong();
	} else if(Ok && crossOver(Stochs,0.79)) {
		Go = mlGoShort();
		if(Train || Go > MLThreshold)
			enterShort();
	}
}

function run()
{
	set(RULES,LOGFILE);	// perceptron rules only, params are fixed
	BarPeriod = 60;
	LookBack = 2500;
	StartDate = 2017;
	EndDate = 20241231;
	NumWFOCycles = 10;
	DataSplit = 85;
	Capital = -10000;

	if(is(INITRUN)) {
		MLThreshold = 0;
		string Cfg = file_content("Data\\mlcfg.txt");
		if(Cfg) MLThreshold = atof(Cfg);
		printf("\nML threshold: %.0f",MLThreshold);
	}

	while(asset(loop("USD/JPY","AUD/USD")))
	while(algo(loop("TRND","CYCL")))
	{
		TimeFrame = 4;
		MaxLong = MaxShort = 1;
		Margin = 300;	// fixed sizing for a clean filter A/B test
		mlFeatures();
		if(strstr(Algo,"TRND")) {
			if(strstr(Asset,"USD/JPY"))
				tradeTrend();
		} else
			tradeCycle();
	}
	return 0;
}
