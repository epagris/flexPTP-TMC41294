/* (C) András Wiesner */

#ifndef OSC_H_
#define OSC_H_

#include <stdbool.h>
#include <stdint.h>

#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom_map.h"

uint32_t g_ui32SysClock; // system clock frequency

void OSC_init(); //initialize oscillator

#endif /* UTILS_OSC_H_ */
