/* (C) András Wiesner, 2020 */

#ifndef SRC_UART_COMM
#define SRC_UART_COMM

#include <stdint.h>

// UART initialization
void UART_init();

// send data through UART
// - if length = 0, then pBuffer-t is treated as a null-terminated string
void UART_send(uint8_t * pBuffer, uint32_t length);
void UART_sendStr(char * pBuffer);

#endif /* SRC_UART_COMM */
