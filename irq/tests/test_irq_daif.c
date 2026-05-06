/*
 * Interrupt Privilege Tests - DAIF Mask Operations
 *
 * Tests DAIF (Debug/SError/IRQ/FIQ) mask bit access across ELs.
 * DAIF modification is typically privileged; EL0 access should trap.
 */
#include "test_framework.h"
#include "sysreg_ops.h"

static volatile uint64_t irq_sink;

/* ---- EL0 test functions ---- */

static void el0_daifset(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(asm volatile("msr daifset, #0xf"));
}

static void el0_daifclr(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(asm volatile("msr daifclr, #0xf"));
}

static void el0_read_daif(uint64_t arg)
{
    (void)arg;
    /*
     * Reading DAIF from EL0 may or may not trap depending on
     * architecture version and SCTLR_EL1.UMA. We arm the trap
     * and accept either outcome.
     */
    EL_DO_TRAP(irq_sink = read_daif());
}

/* ============================================================
 * IRQ-DAIF-01: DAIFSET from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_irq_daifset_at_el1);
bool test_irq_daifset_at_el1(void)
{
    TEST_BEGIN("IRQ-DAIF-01: MSR DAIFSET @ EL1 → ok");
    EXPECT_NO_TRAP(asm volatile("msr daifset, #0xf"));
    TEST_END();
}

/* ============================================================
 * IRQ-DAIF-02: DAIFCLR from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_irq_daifclr_at_el1);
bool test_irq_daifclr_at_el1(void)
{
    TEST_BEGIN("IRQ-DAIF-02: MSR DAIFCLR @ EL1 → ok");
    EXPECT_NO_TRAP(asm volatile("msr daifclr, #0xf"));
    /* Re-mask interrupts for safety */
    asm volatile("msr daifset, #0xf");
    TEST_END();
}

/* ============================================================
 * IRQ-DAIF-03: MRS DAIF from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_irq_read_daif_at_el1);
bool test_irq_read_daif_at_el1(void)
{
    TEST_BEGIN("IRQ-DAIF-03: MRS DAIF @ EL1 → ok");
    EXPECT_NO_TRAP(irq_sink = read_daif());
    TEST_END();
}

/* ============================================================
 * IRQ-DAIF-04: DAIFSET from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_irq_daifset_at_el0);
bool test_irq_daifset_at_el0(void)
{
    TEST_BEGIN("IRQ-DAIF-04: MSR DAIFSET @ EL0 → trap");
    run_at_el(EL0, el0_daifset, 0);
    CHECK_TRAP("daifset from EL0", EC_MSR_MRS_SYSTEM);
    TEST_END();
}

/* ============================================================
 * IRQ-DAIF-05: DAIFCLR from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_irq_daifclr_at_el0);
bool test_irq_daifclr_at_el0(void)
{
    TEST_BEGIN("IRQ-DAIF-05: MSR DAIFCLR @ EL0 → trap");
    run_at_el(EL0, el0_daifclr, 0);
    CHECK_TRAP("daifclr from EL0", EC_MSR_MRS_SYSTEM);
    TEST_END();
}

/* ============================================================
 * IRQ-DAIF-06: DAIF read from EL0 behavior check
 * ============================================================ */
TEST_REGISTER(test_irq_read_daif_at_el0);
bool test_irq_read_daif_at_el0(void)
{
    TEST_BEGIN("IRQ-DAIF-06: MRS DAIF @ EL0 → trap/ok (impl-defined)");
    run_at_el(EL0, el0_read_daif, 0);
    /*
     * DAIF read at EL0: behavior depends on SCTLR_EL1.UMA.
     * If UMA=0 (default), EL0 access to interrupt masks traps.
     * We just verify no crash and record the outcome.
     */
    if (trap_record.triggered) {
        printf("(trapped as expected) ");
    } else {
        printf("(allowed by config) ");
    }
    trap_record_reset();
    TEST_END();
}
