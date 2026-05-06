/*
 * ARM Privilege Test Framework - Trap Handler Interface
 *
 * Provides the "armed trap" mechanism: test code marks an expected
 * exception, the handler records it, and the test asserts on the result.
 */
#ifndef _TRAP_H_
#define _TRAP_H_

#include "types.h"

/* Exception type constants (must match trap_asm.S) */
#define EXC_SYNC    0
#define EXC_IRQ     1
#define EXC_FIQ     2
#define EXC_SERROR  3

/* Exception source constants */
#define SRC_CUR_SP0     0
#define SRC_CUR_SPX     1
#define SRC_LOW_A64     2
#define SRC_LOW_A32     3

/* Trap record: captures exception details when armed */
typedef struct {
    volatile bool armed;        /* Whether we expect an exception */
    volatile bool triggered;    /* Whether an exception was caught */
    uint64_t esr;               /* ESR_ELx value */
    uint64_t elr;               /* ELR_ELx value (faulting PC) */
    uint64_t far_el;            /* FAR_ELx value (fault address) */
    uint32_t exception_class;   /* EC field from ESR */
    uint32_t target_el;         /* Which EL handled the trap */
    uint32_t exc_type;          /* sync/irq/fiq/serror */
    uint32_t exc_source;        /* cur_sp0/cur_spx/low64/low32 */
} trap_record_t;

/* Global trap record */
extern trap_record_t trap_record;

/* Arm/disarm the trap mechanism */
void trap_expect_begin(void);
void trap_expect_end(void);
void trap_record_reset(void);

/* C handlers called from trap_asm.S */
void el1_trap_handler(uint64_t exc_type, uint64_t exc_source);
void el2_trap_handler(uint64_t exc_type, uint64_t exc_source);
void el3_trap_handler(uint64_t exc_type, uint64_t exc_source);

/* Read current EL (0-3) */
static inline uint32_t get_current_el(void)
{
    uint64_t val;
    asm volatile("mrs %0, CurrentEL" : "=r"(val));
    return (uint32_t)((val >> 2) & 0x3);
}

#endif /* _TRAP_H_ */
