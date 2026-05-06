/*
 * Platform Behavior Model - Generic Hardware Default
 * Falls back to wfi loop for halt.
 */
#ifndef _PLATFORM_MODEL_H_
#define _PLATFORM_MODEL_H_

#define PLATFORM_HALT_PASS() do {           \
    while(1) { asm volatile("wfi"); }       \
} while(0)

#define PLATFORM_HALT_FAIL() do {           \
    while(1) { asm volatile("wfi"); }       \
} while(0)

#define PLATFORM_BOOT()

#endif /* _PLATFORM_MODEL_H_ */
