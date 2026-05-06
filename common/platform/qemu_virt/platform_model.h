/*
 * Platform Behavior Model - QEMU virt
 *
 * Defines how the platform boots and halts.
 * QEMU virt uses sysreg device at 0x09010000 for orderly shutdown.
 */
#ifndef _PLATFORM_MODEL_H_
#define _PLATFORM_MODEL_H_

/* QEMU sysreg finisher: ADP_Stopped_ApplicationExit */
#define QEMU_SYSREG_FINISHER   (*(volatile unsigned int *)(0x09010000UL + 0x18))

#define PLATFORM_HALT_PASS() do {           \
    QEMU_SYSREG_FINISHER = 0x20026;         \
    while(1) { asm volatile("wfi"); }       \
} while(0)

#define PLATFORM_HALT_FAIL() do {           \
    QEMU_SYSREG_FINISHER = 0x20024;         \
    while(1) { asm volatile("wfi"); }       \
} while(0)

#define PLATFORM_BOOT()     /* No special early boot on QEMU */

#endif /* _PLATFORM_MODEL_H_ */
