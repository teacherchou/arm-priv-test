/*
 * ARM Privilege Test Framework - UART Driver Interface
 * Supports PL011 (QEMU virt) and NS16550 style UARTs.
 */
#ifndef _UART_H_
#define _UART_H_

#include "types.h"

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(uint64_t val);
void uart_putdec(int64_t val);

/* Minimal printf (subset: %s, %d, %x, %lx, %p, %c, %%) */
void printf(const char *fmt, ...);

#endif /* _UART_H_ */
