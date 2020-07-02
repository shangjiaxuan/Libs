#include <Windows.h>
#include <iostream>

#pragma comment( lib, "Winmm" )

int main()
{
	TIMECAPS caps;
	timeGetDevCaps(&caps,sizeof(caps));
	std::cout<<"PeriodMin:\t"<<caps.wPeriodMin<<"\nPeriodMax:\t"<<caps.wPeriodMax<<std::endl;
	return 0;
}
