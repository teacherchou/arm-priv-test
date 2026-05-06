/*
 * Platform Configuration - QEMU AArch64 virt machine
 */
#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#define PLATFORM_NAME           "QEMU virt (AArch64)"

/* PL011 UART */
#define PLATFORM_UART0_BASE     0x09000000UL

/* Memory layout */
#define PLATFORM_MEM_BASE       0x40000000UL
#define PLATFORM_MEM_SIZE       0x10000000UL    /* 256MB */

/* GICv3 */
#define PLATFORM_GICD_BASE      0x08000000UL    /* Distributor */
#define PLATFORM_GICR_BASE      0x080A0000UL    /* Redistributor */

/* QEMU virt sysreg finisher for halt */
#define PLATFORM_SYSREG_BASE    0x09010000UL

/* Generic Timer frequency (QEMU default: 62.5 MHz) */
#define PLATFORM_TIMER_FREQ     62500000UL

#endif /* _PLATFORM_H_ */
