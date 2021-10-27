/* (C) András Wiesner, 2020 */

#ifndef TASKS_USER_TASKS_H_
#define TASKS_USER_TASKS_H_

#include <stdint.h>
#include <stdbool.h>
#include "utils/uartstdio.h"

// common includes
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"

#include "utils/lwiplib.h"
#include "utils/locator.h"

#include "drivers/pinout.h"
#include "drivers/eth_client_lwip.h"

#include "utils.h"

extern uint32_t g_ui32SysClock;

void reg_task_eth(); // register ETH task
void reg_task_ptp(); // register PTP task
void unreg_task_ptp();  // unregister PTP task
void reg_task_cli(); // register CLI task
void unreg_task_cli(); // unregister CLI task

#endif /* TASKS_USER_TASKS_H_ */
