// EhlersML: Ehlers DSP algos + Perceptron GO/SKIP filter + OptimalF ///
// Evaluation Shell strategy, pattern after Workshop6c
#include <evars.h>
// TRND algo - trend following in trending regime
var TRND_Period;	//= 100, 40..200, 20; Decycle cutoff period in bars
var TRND_CTIPeriod;	//= 60, 20..120; Correlation Trend Indicator period
var TRND_CTIThresh;	//= 0.4, 0.2..0.8, 0.1; CTI trend threshold
// CYCL algo - counter trend in cycle regime
var CYCL_Period;	//= 20, 10..40, 5; StochEhlers stochastic period
var CYCL_CutLow;	//= 10, 6..16; Roofing filter lowpass cutoff
var CYCL_CutHigh;	//= 48, 30..80; Roofing filter highpass cutoff
var CYCL_Edge;		//= 0.2, 0.1..0.35, 0.05; Entry threshold distance from 0/1
var CYCL_TrailDist;	//= 4, 0..8; Trailing distance in ATR units
// common variables
var MMIPeriod;		//= 300, 100..500; MMI period for the ML feature
var StopDist;		//= 8, 4..20, 2; Stop distance in ATR units
var ATRPeriod;		//= 100, 30..150; ATR period for stop/trail distances
var MLThreshold;	//= 0, -100..100; Perceptron GO/SKIP threshold, -100 = filter off
var MLMode;		//= 0, 0..1; 0 - no ML; 1 - perceptron filter, train only with fixed params (all steps 0)!
var SessionStart;	//= 0, 0..21, 3; Entries allowed from hour (UTC)
var SessionEnd;		//= 24, 3..24, 3; Entries allowed until hour (UTC)
var BarShift;		//= 0, 0..3; H4 frame offset in H1 bars
var RiskDayLoss;	//= 0, 0..10; Daily loss halt in % of equity, 0 = off
var RiskMaxDD;		//= 0, 0..50; Equity drawdown halt in % from peak, 0 = off
END_OF_VARS

#define _ASSETS	"EUR/USD","GBP/USD","USD/JPY","USD/CHF","AUD/USD","USD/CAD","EUR/CHF","EUR/JPY","GBP/JPY"
#define _ALGOS	"TRND","CYCL","TRN2","CYC2"
#define _TIMEFRAMES	60,120,240

// normalized regime features for the perceptron filter
var FeedMMI,FeedCTI,FeedFisher,FeedDC,FeedVol,FeedTrend;

void mlFeatures()
{
	vars Prices = series(price(0));
	FeedMMI = MMI(Prices,V.MMIPeriod)/100. - 0.5;	// market meanness
	FeedCTI = CTI(Prices,V.TRND_CTIPeriod);		// trend correlation -1..1
	FeedFisher = FisherN(Prices,100);		// price stretch, ~ -3..3
	FeedDC = HTDcPeriod(Prices)/25. - 1.;		// dominant cycle period
	FeedVol = ATR(30)/ATR(200) - 1.;		// volatility regime
	FeedTrend = HTTrendMode(Prices) - 0.5;		// trend/cycle mode
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

// dynamic risk guard + entry session window ///////////////////
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
	if(V.RiskDayLoss > 0 && Eq < DayStartEquity - 0.01*V.RiskDayLoss*abs(DayStartEquity))
		RiskHalt = 1;	// daily loss limit hit
	if(V.RiskMaxDD > 0 && Eq < PeakEquity - 0.01*V.RiskMaxDD*abs(PeakEquity))
		RiskHalt = 2;	// max drawdown hit, stays halted
	if(RiskHalt) {	// flatten and block entries
		exitLong("*");
		exitShort("*");
	}

	var SStart = _optimize(V.SessionStart);
	var SEnd = _optimize(V.SessionEnd);
	int H = hour(0);
	int SOk;
	if(SStart <= SEnd)
		SOk = (H >= SStart && H < SEnd);
	else	// window crosses midnight
		SOk = (H >= SStart || H < SEnd);
	EntryOK = SOk && !RiskHalt;
}

void tradeTrend()
{
	vars Prices = series(price(0));
	vars Trends = series(Decycle(Prices,_optimize(V.TRND_Period)));
	vars CTIs = series(CTI(Prices,V.TRND_CTIPeriod));
	var Threshold = _optimize(V.TRND_CTIThresh);

	Stop = _optimize(V.StopDist) * ATR(V.ATRPeriod);
	Trail = 0;

	var Go = 100;
	if(EntryOK && CTIs[0] > Threshold && valley(Trends)) {	// dip buy in uptrend
		if(V.MLMode > 0) Go = mlGoLong();
		if(Train || Go > V.MLThreshold)	// filter only in Test/Trade
			enterLong();
	} else if(EntryOK && CTIs[0] < -Threshold && peak(Trends)) {
		if(V.MLMode > 0) Go = mlGoShort();
		if(Train || Go > V.MLThreshold)
			enterShort();
	}
}

void tradeCycle()
{
	vars Prices = series(price(0));
	vars Stochs = series(StochEhlers(Prices,
		_optimize(V.CYCL_Period),V.CYCL_CutLow,V.CYCL_CutHigh));
	var Edge = _optimize(V.CYCL_Edge);

	Stop = _optimize(V.StopDist) * ATR(V.ATRPeriod);
	Trail = V.CYCL_TrailDist * ATR(V.ATRPeriod);

	var Go = 100;
	if(EntryOK && crossUnder(Stochs,Edge)) {	// oversold, expect swing up
		if(V.MLMode > 0) Go = mlGoLong();
		if(Train || Go > V.MLThreshold)	// filter only in Test/Trade
			enterLong();
	} else if(EntryOK && crossOver(Stochs,1.-Edge)) {
		if(V.MLMode > 0) Go = mlGoShort();
		if(Train || Go > V.MLThreshold)
			enterShort();
	}
}

void tradeTrend2()	// TRND entries + trailing exit instead of reverse-only
{
	vars Prices = series(price(0));
	vars Trends = series(Decycle(Prices,_optimize(V.TRND_Period)));
	vars CTIs = series(CTI(Prices,V.TRND_CTIPeriod));
	var Threshold = _optimize(V.TRND_CTIThresh);

	Stop = _optimize(V.StopDist) * ATR(V.ATRPeriod);
	Trail = V.CYCL_TrailDist * ATR(V.ATRPeriod);	// protect trend profits

	var Go = 100;
	if(EntryOK && CTIs[0] > Threshold && valley(Trends)) {
		if(V.MLMode > 0) Go = mlGoLong();
		if(Train || Go > V.MLThreshold)
			enterLong();
	} else if(EntryOK && CTIs[0] < -Threshold && peak(Trends)) {
		if(V.MLMode > 0) Go = mlGoShort();
		if(Train || Go > V.MLThreshold)
			enterShort();
	}
}

void tradeCycle2()	// CYCL entries + exit at the midline (target the mean)
{
	vars Prices = series(price(0));
	vars Stochs = series(StochEhlers(Prices,
		_optimize(V.CYCL_Period),V.CYCL_CutLow,V.CYCL_CutHigh));
	var Edge = _optimize(V.CYCL_Edge);

	Stop = _optimize(V.StopDist) * ATR(V.ATRPeriod);
	Trail = 0;

	if(crossOver(Stochs,0.5))	// swing reached the mean - take profit
		exitLong();
	if(crossUnder(Stochs,0.5))
		exitShort();

	var Go = 100;
	if(EntryOK && crossUnder(Stochs,Edge)) {
		if(V.MLMode > 0) Go = mlGoLong();
		if(Train || Go > V.MLThreshold)
			enterLong();
	} else if(EntryOK && crossOver(Stochs,1.-Edge)) {
		if(V.MLMode > 0) Go = mlGoShort();
		if(Train || Go > V.MLThreshold)
			enterShort();
	}
}

function run()
{
	set(PARAMETERS,FACTORS);	// optimized parameters and OptimalF factors
	set(TESTNOW,PLOTNOW,LOGFILE);
	StartDate = 2017;	// fixed simulation period, keep same for all jobs
	EndDate = 20241231;	// GBP/JPY history incomplete in 2025
	BarPeriod = 60;
	LookBack = 2500;	// MMI 500 * TimeFrame 4 + headroom
	// NumCores comes from the _Max_Threads panel row - don't set it here
	Capital = -10000;	// reinvestment mode

	if(ReTrain) {
		UpdateDays = -1;
		SelectWFO = -1;
		set(FACTORS|OFF);
	}

	assetLoop();	// select asset, algo, timeframe of the current job

	// ML pass: rules only - combined RULES+PARAMETERS training does not
	// work with asset/algo components, so train MLMode=1 jobs with all
	// parameter steps at 0 (fixed values from the MLMode=0 pass)
	if(V.MLMode > 0) set(RULES);
	else set(RULES|OFF);

	MaxLong = MaxShort = 1;
	Margin = 0.5 * OptimalF * abs(Capital);	// OptimalF capital allocation
	FrameOffset = (int)V.BarShift;	// H4 frame alignment in H1 bars

	riskGuard();	// daily loss / drawdown halt + session window
	if(V.MLMode > 0) mlFeatures();
	if(strstr(Algo,"TRND"))
		tradeTrend();
	else if(strstr(Algo,"CYCL"))
		tradeCycle();
	else if(strstr(Algo,"TRN2"))
		tradeTrend2();
	else if(strstr(Algo,"CYC2"))
		tradeCycle2();
	return 0;
}

#include <eval.c>
