/* (C) András Wiesner, 2020 */

#include "uart_comm.h"

#include <string.h>
#include <stdbool.h>

#include "driverlib/uart.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "utils.h"
#include "hw_memmap.h"

#include "utils/uartstdio.h"

extern uint32_t g_ui32SysClock;

void UART_init() {
    EnableWaitPeripheral(SYSCTL_PERIPH_GPIOA); // enable GPIOA
	EnableWaitPeripheral(SYSCTL_PERIPH_UART0); // enable UART0
	GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1); // setup pins
	
	GPIOPinConfigure(GPIO_PA0_U0RX);
	GPIOPinConfigure(GPIO_PA1_U0TX);

	UARTClockSourceSet(UART0_BASE, UART_CLOCK_SYSTEM);
	UARTStdioConfig(0, 115200, g_ui32SysClock);
}

void UART_send(uint8_t * pBuffer, uint32_t length) {
 	if (length == 0) {
		length = strlen((char *) pBuffer);
	}

	uint32_t transmitted = 0;

	while (transmitted != length) {
		bool ret = UARTCharPutNonBlocking(UART0_BASE, pBuffer[transmitted]);

		if (ret == true) {
			transmitted++;
		}
	}   
}

void UART_sendStr(char * pBuffer) {
    UART_send((uint8_t *) pBuffer, 0);
}
