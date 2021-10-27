/* (C) András Wiesner, 2020 */

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"

// oscillator init function
#include "osc.h"

#include "utils.h"

// UART communication
#include "uart_comm.h"
#include "drivers/pinout.h"

// task management
#include "tasks/user_tasks.h"

// global initialization
void init() {
	OSC_init(); // initializing oscillator
	UART_init(); // UART initialization (115200bps)
}

// task registration
void reg_tasks() {
	reg_task_eth(); // registering eth task
	reg_task_cli(); // registering cli task
}

int main(void)
{
	init(); // initialization

	// configuring Ethernet pins
    PinoutSet(true, false);

	// --------

    MSG("\033[2J\033[H");
    MSG("Registering tasks...\r\n");

	reg_tasks(); // registering tasks

	// ---------

	// starting scheduler
	MSG("Starting scheduler...\r\n");

	vTaskStartScheduler();
}



//*****************************************************************************
void
vApplicationStackOverflowHook(xTaskHandle *pxTask, char *pcTaskName)
{

	UARTprintf("Stack overflow in task: %s\r\n", pcTaskName);

    while(1)
    {
    }
}

// runtime statistics...
volatile unsigned long g_vulRunTimeStatsCountValue;

void __error__(char *pcFilename, uint32_t ui32Line) {
	UARTprintf("%s:%d\r\n", pcFilename, ui32Line);
}
