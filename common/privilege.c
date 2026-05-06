/*
 * ARM Privilege Test Framework - Exception Level Switch Implementation
 *
 * EL switching mechanism:
 *   Down (EL1 → EL0): Save EL1 return context, ERET to EL0 trampoline
 *   Up (EL0 → EL1):   SVC #1 → trap handler restores saved EL1 context
 *   Up (EL1 → EL2):   HVC #2 → traps to EL2, handler runs fn, ERET back
 *   Up (EL1 → EL3):   SMC #3 → traps to EL3, handler runs fn, ERET back
 *
 * For EL0 roundtrip: _goto_el0 uses ERET which does NOT return to its caller.
 * So we must save the EL1 return address + SP before ERET, and the trap
 * handler for SVC #1 restores them so ERET goes back to the right place.
 *
 * For EL2/EL3 roundtrip: HVC/SMC are synchronous exceptions that trap to
 * the higher EL. The higher EL handler processes the roundtrip protocol
 * (HVC #2 / SMC #3) and ERETs back to EL1 at the saved return address.
 */
#include "privilege.h"
#include "trap.h"
#include "uart.h"

/* EL0 stack (separate from EL1 stack) */
static uint8_t el0_stack[4096] __attribute__((aligned(16)));

/* Saved EL1 context for SVC return */
volatile uint64_t _el1_return_addr;
volatile uint64_t _el1_return_sp;

/*
 * Trampoline state for run_at_el.
 * For EL2/EL3 roundtrips, the higher-EL trap handler reads these
 * globals, calls fn(arg), then ERETs back to EL1.
 */
void (* volatile _trampoline_fn)(uint64_t);
volatile uint64_t _trampoline_arg;

static void el0_trampoline(void)
{
    if (_trampoline_fn)
        ((void (*)(uint64_t))_trampoline_fn)(_trampoline_arg);

    /* Return to EL1 via SVC */
    asm volatile("svc #0x0001");

    /* Should never reach here */
    while (1)
        ;
}

/*
 * Internal: drop to EL0, run trampoline, return here after SVC #1.
 * Uses setjmp-like pattern: saves LR and SP, then ERET to EL0.
 * Trap handler restores them on SVC #1.
 */
static void __attribute__((noinline)) do_el0_roundtrip(void)
{
    uint64_t el0_sp = (uint64_t)&el0_stack[sizeof(el0_stack)];

    /*
     * Save the return address (LR) and current SP.
     * After SVC #1, the trap handler will set ELR_EL1 = _el1_return_addr
     * and SPSR to EL1h, so ERET returns here.
     */
    asm volatile(
        "adr    %0, 1f\n"
        "mov    %1, sp\n"
        : "=r"(_el1_return_addr), "=r"(_el1_return_sp)
    );

    _goto_el0((uint64_t)el0_trampoline, el0_sp);

    /* This label is where we return to after SVC #1 */
    asm volatile("1:");
}

/*
 * EL1 → EL2 roundtrip via HVC #2.
 *
 * HVC is a synchronous exception that traps to EL2. The EL2 handler
 * recognizes HVC_RETURN_TO_EL1 (imm16=0x0002) as the roundtrip protocol:
 *   - If _trampoline_fn is set, call it at EL2, then ERET back to EL1
 *   - Otherwise, just ERET back to EL1 immediately
 *
 * Unlike EL0 roundtrip, HVC traps synchronously — ELR_EL2 points to
 * the instruction after HVC, so the handler just needs to advance past
 * it and ERET. The handler stub in trap_asm.S saves/restores all regs.
 */
static void __attribute__((noinline)) do_el2_roundtrip(void)
{
    asm volatile("hvc #0x0002");
}

/*
 * EL1 → EL3 roundtrip via SMC #3.
 * Same pattern as EL2 roundtrip but traps to EL3.
 */
static void __attribute__((noinline)) do_el3_roundtrip(void)
{
    asm volatile("smc #0x0003");
}

void goto_el(uint32_t target_el)
{
    uint32_t current = get_current_el();

    if (target_el == current)
        return;

    if (current == EL1 && target_el == EL0) {
        _trampoline_fn = NULL;
        _trampoline_arg = 0;
        do_el0_roundtrip();
        return;
    }

    if (current == EL0 && target_el == EL1) {
        asm volatile("svc #0x0001");
        return;
    }

    if (current == EL1 && target_el == EL2) {
        _trampoline_fn = NULL;
        _trampoline_arg = 0;
        do_el2_roundtrip();
        return;
    }

    if (current == EL1 && target_el == EL3) {
        _trampoline_fn = NULL;
        _trampoline_arg = 0;
        do_el3_roundtrip();
        return;
    }

    printf("ERROR: goto_el(%d) from EL%d not supported\n",
           target_el, current);
}

void run_at_el(uint32_t target_el, void (*fn)(uint64_t), uint64_t arg)
{
    uint32_t current = get_current_el();

    if (target_el == current) {
        fn(arg);
        return;
    }

    if (current == EL1 && target_el == EL0) {
        _trampoline_fn = fn;
        _trampoline_arg = arg;
        do_el0_roundtrip();
        return;
    }

    if (current == EL1 && target_el == EL2) {
        _trampoline_fn = fn;
        _trampoline_arg = arg;
        do_el2_roundtrip();
        return;
    }

    if (current == EL1 && target_el == EL3) {
        _trampoline_fn = fn;
        _trampoline_arg = arg;
        do_el3_roundtrip();
        return;
    }

    printf("ERROR: run_at_el(%d) from EL%d not supported\n",
           target_el, current);
}
