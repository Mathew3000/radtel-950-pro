#ifndef RT950_UART_H
#define RT950_UART_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Programming cable port: UART4 on PC10 (TX) and PC11 (RX), 115200 8N1, the
 * settings the OEM updater (RT-950_EnUPDATE, FormMain.cs) opens the cable
 * with. Received bytes go into a small ring buffer from the RX interrupt.
 */
void uart_init(void);
bool uart_getc(uint8_t *c);
void uart_putc(uint8_t c);

#endif
