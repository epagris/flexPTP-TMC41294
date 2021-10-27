/* (C) András Wiesner */

#include "osc.h"

#include "driverlib/sysctl.h"

void OSC_init() {
	SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);
	#ifdef PART_TM4C123GE6PM
		SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_XTAL_16MHZ | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN); // 80 MHz
		g_ui32SysClock = SysCtlClockGet();
	#elif PART_TM4C1294NCPDT
		g_ui32SysClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
											SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000); // 120 MHz
	#endif
}
