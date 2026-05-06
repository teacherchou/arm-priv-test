/*
 * Platform Configuration - Generic Hardware Default
 * Override these values for your specific board.
 */
#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#define PLATFORM_NAME           "hw_default (AArch64)"
#define PLATFORM_UART0_BASE     0x09000000UL
#define PLATFORM_MEM_BASE       0x40000000UL
#define PLATFORM_MEM_SIZE       0x10000000UL
#define PLATFORM_GICD_BASE      0x08000000UL
#define PLATFORM_GICR_BASE      0x080A0000UL
#define PLATFORM_TIMER_FREQ     62500000UL

#endif /* _PLATFORM_H_ */
