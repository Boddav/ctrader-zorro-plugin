// Montecarlo Reality Check: USD/JPY TRND H4 from EhlersML
// cycle 1 = original prices, cycles 2..N = shuffled prices
// results appended to Log\mrc_results.csv
#define MRC_RUNS	21

function run()
{
	set(PARAMETERS,FACTORS,LOGFILE);
	NumTotalCycles = MRC_RUNS;
	BarPeriod = 60;
	LookBack = 2500;
	StartDate = 2019;
	EndDate = 20241231;
	Capital = -10000;
	NumWFOCycles = 10;
	DataSplit = 85;

	set(PRELOAD);	// reload history at any cycle
	if(TotalCycle > 1)
		setf(Detrend,SHUFFLE);	// randomize the price curve
	else
		resf(Detrend,SHUFFLE);	// original run

	asset("USD/JPY");
	algo("TRND");
	TimeFrame = 4;
	MaxLong = MaxShort = 1;

	vars Prices = series(price(0));
	vars Trends = series(Decycle(Prices,optimize("Period",100,40,200,20)));
	vars CTIs = series(CTI(Prices,60));
	var Threshold = optimize("CTIThresh",0.4,0.2,0.8,0.1);
	Stop = optimize("StopDist",8,4,20,2) * ATR(100);
	Trail = 0;
	Margin = 0.5 * OptimalF * abs(Capital);

	if(CTIs[0] > Threshold && valley(Trends))
		enterLong();
	else if(CTIs[0] < -Threshold && peak(Trends))
		enterShort();

	if(is(EXITRUN)) {
		var PF = 9.99;
		if(LossTotal > 0) PF = WinTotal/LossTotal;
		file_append("Log\\mrc_results.csv",
			strf("%i,%.0f,%.2f,%i\n",TotalCycle,WinTotal-LossTotal,PF,NumWinTotal+NumLossTotal),0);
	}
	return 0;
}
