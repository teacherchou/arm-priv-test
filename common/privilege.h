/*
 * ARM Privilege Test Framework - Exception Level Switch Interface
 *
 * Provides bidirectional EL switching:
 *   Down: EL1 → EL0 via ERET
 *   Up:   EL0 → EL1 via SVC
 *
 * Usage pattern in tests:
 *   goto_el(EL0);         // switch to EL0
 *   ... do test ...       // executes at EL0, traps go to EL1
 *   goto_el(EL1);         // return to EL1 via SVC
 */
#ifndef _PRIVILEGE_H_
#define _PRIVILEGE_H_

#include "types.h"
#include "encoding.h"

/*
 * Switch to the specified exception level.
 *
 * Downward (EL1 → EL0): Uses ERET with SPSR set to EL0t.
 * Upward (EL0 → EL1):   Uses SVC #0, which traps to EL1 handler.
 *
 * Note: EL2/EL3 switches are available but require special setup.
 */
void goto_el(uint32_t target_el);

/*
 * Execute a function at the specified EL, then return.
 * The function signature must be: void fn(uint64_t arg)
 *
 * This is a convenience wrapper that:
 *   1. Switches to target_el
 *   2. Calls fn(arg)
 *   3. Switches back to the original EL
 */
void run_at_el(uint32_t target_el, void (*fn)(uint64_t), uint64_t arg);

/* Assembly helpers: switch from EL1 to lower/higher ELs (defined in trap_asm.S) */
extern void _goto_el0(uint64_t target_pc, uint64_t el0_sp);

/* EL switching protocol immediate values */
#define SVC_RETURN_TO_EL1  0x0001   /* SVC #1: EL0 → EL1 return */
#define HVC_RETURN_TO_EL1  0x0002   /* HVC #2: EL2 → EL1 return (roundtrip protocol) */
#define SMC_RETURN_TO_EL1  0x0003   /* SMC #3: EL3 → EL1 return (roundtrip protocol) */

/*
 * Saved EL1 return address and stack pointer.
 * When SVC #1 / HVC #2 / SMC #3 is handled, the trap handler sets ELR
 * to this address and restores SP, so ERET returns directly to the caller.
 */
extern volatile uint64_t _el1_return_addr;
extern volatile uint64_t _el1_return_sp;

/*
 * Trampoline function pointer and argument for EL2/EL3 roundtrips.
 * When run_at_el(EL2/EL3, fn, arg) is called, these are set before
 * HVC/SMC. The higher-EL trap handler calls fn(arg) at its EL,
 * then ERETs back to EL1.
 */
extern void (* volatile _trampoline_fn)(uint64_t);
extern volatile uint64_t _trampoline_arg;

#endif /* _PRIVILEGE_H_ */
