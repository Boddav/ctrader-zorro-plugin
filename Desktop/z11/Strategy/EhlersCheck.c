// Standalone WFO robustness check: USD/JPY TRND H4 from EhlersML
// WFO config read from Data\checkcfg.txt: "cycles,datasplit" e.g. "8,75"
int CCycles = 10;
var CSplit = 85;

function run()
{
	set(PARAMETERS,FACTORS,LOGFILE);
	BarPeriod = 60;
	LookBack = 2500;
	StartDate = 2019;	// USD/JPY history starts 2019
	EndDate = 20241231;
	Capital = -10000;	// reinvestment mode

	if(is(INITRUN)) {
		string Cfg = file_content("Data\\checkcfg.txt");
		if(Cfg) {
			CCycles = atoi(Cfg);
			string P = strchr(Cfg,',');
			if(P) CSplit = atof(P+1);
		}
		printf("\nWFO config: %i cycles, split %.0f",CCycles,CSplit);
	}
	NumWFOCycles = CCycles;
	DataSplit = CSplit;

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
	return 0;
}
