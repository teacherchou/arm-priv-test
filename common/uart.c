/*
 * ARM Privilege Test Framework - UART Driver Implementation
 *
 * PL011 UART driver for QEMU virt platform.
 * Other platforms can override via platform.h macros.
 */
#include "uart.h"
#include "types.h"

/* These are provided by platform.h via -include */
#ifndef PLATFORM_UART0_BASE
#define PLATFORM_UART0_BASE 0x09000000UL  /* QEMU virt PL011 default */
#endif

/* PL011 register offsets */
#define UART_DR     0x000   /* Data Register */
#define UART_FR     0x018   /* Flag Register */
#define UART_IBRD   0x024   /* Integer Baud Rate */
#define UART_FBRD   0x028   /* Fractional Baud Rate */
#define UART_LCR_H  0x02C  /* Line Control */
#define UART_CR     0x030   /* Control Register */
#define UART_ICR    0x044   /* Interrupt Clear */

#define UART_FR_TXFF    (1 << 5)  /* TX FIFO full */

#define UART_REG(offset) \
    (*(volatile uint32_t *)(PLATFORM_UART0_BASE + (offset)))

void uart_init(void)
{
    /* Disable UART */
    UART_REG(UART_CR) = 0;

    /* Clear all interrupts */
    UART_REG(UART_ICR) = 0x7FF;

    /* Set baud rate (QEMU doesn't care about actual value) */
    UART_REG(UART_IBRD) = 1;
    UART_REG(UART_FBRD) = 0;

    /* 8N1, enable FIFO */
    UART_REG(UART_LCR_H) = (3 << 5) | (1 << 4);

    /* Enable UART, TX, RX */
    UART_REG(UART_CR) = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putc(char c)
{
    /* Wait until TX FIFO is not full */
    while (UART_REG(UART_FR) & UART_FR_TXFF)
        ;
    UART_REG(UART_DR) = (uint32_t)c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

static const char hex_digits[] = "0123456789abcdef";

void uart_puthex(uint64_t val)
{
    uart_puts("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        int nibble = (val >> i) & 0xF;
        if (nibble || started || i == 0) {
            uart_putc(hex_digits[nibble]);
            started = 1;
        }
    }
}

void uart_putdec(int64_t val)
{
    if (val < 0) {
        uart_putc('-');
        val = -val;
    }
    if (val == 0) {
        uart_putc('0');
        return;
    }
    char buf[20];
    int idx = 0;
    while (val > 0) {
        buf[idx++] = '0' + (val % 10);
        val /= 10;
    }
    while (--idx >= 0)
        uart_putc(buf[idx]);
}

/* Minimal variadic printf implementation */
typedef __builtin_va_list va_list;
#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

void printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            if (*fmt == '\n')
                uart_putc('\r');
            uart_putc(*fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        /* Handle 'l' length modifier */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
        }

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            uart_puts(s ? s : "(null)");
            break;
        }
        case 'd': {
            int64_t v = is_long ? va_arg(ap, int64_t)
                                : (int64_t)va_arg(ap, int);
            uart_putdec(v);
            break;
        }
        case 'u': {
            uint64_t v = is_long ? va_arg(ap, uint64_t)
                                 : (uint64_t)va_arg(ap, unsigned int);
            /* Print as unsigned decimal */
            if (v == 0) {
                uart_putc('0');
            } else {
                char buf[20];
                int idx = 0;
                while (v > 0) {
                    buf[idx++] = '0' + (v % 10);
                    v /= 10;
                }
                while (--idx >= 0)
                    uart_putc(buf[idx]);
            }
            break;
        }
        case 'x':
        case 'p': {
            uint64_t v = is_long ? va_arg(ap, uint64_t)
                                 : (uint64_t)va_arg(ap, unsigned int);
            if (*fmt == 'p')
                uart_puts("0x");
            /* Print hex without 0x prefix for %x */
            {
                int started = 0;
                for (int i = 60; i >= 0; i -= 4) {
                    int nibble = (v >> i) & 0xF;
                    if (nibble || started || i == 0) {
                        uart_putc(hex_digits[nibble]);
                        started = 1;
                    }
                }
            }
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            uart_putc(c);
            break;
        }
        case '%':
            uart_putc('%');
            break;
        default:
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }
        fmt++;
    }

    va_end(ap);
}
