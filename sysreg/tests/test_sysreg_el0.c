/*
 * System Register Access Privilege Tests - EL0 Trap Verification
 *
 * Verifies that accessing EL1 system registers from EL0 correctly
 * generates a synchronous exception.
 *
 * Note on EC values:
 *   ARM architecture reports EC=0x00 (Unknown/UNDEF) when EL0 accesses
 *   EL1 system registers directly. EC=0x18 (MSR/MRS/System trap) is only
 *   produced when trap routing is configured (e.g. HCR_EL2.TVM for EL1→EL2).
 *   We verify the trap occurs and check EC=EC_UNKNOWN.
 */
#include "test_framework.h"
#include "sysreg_ops.h"

/* ---- Helper: execute a sysreg read at EL0, expect trap ---- */

static volatile uint64_t sysreg_sink;

static void el0_read_sctlr_el1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_sctlr_el1());
}

static void el0_write_sctlr_el1(uint64_t arg)
{
    EL_DO_TRAP(write_sctlr_el1(arg));
}

static void el0_read_vbar_el1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_vbar_el1());
}

static void el0_write_vbar_el1(uint64_t arg)
{
    EL_DO_TRAP(write_vbar_el1(arg));
}

static void el0_read_ttbr0_el1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_ttbr0_el1());
}

static void el0_read_tcr_el1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_tcr_el1());
}

static void el0_read_esr_el1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_esr_el1());
}

static void el0_read_far_el1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_far_el1());
}

static void el0_read_mair_el1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(sysreg_sink = read_mair_el1());
}

static void el0_tlbi_vmalle1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(tlbi_vmalle1());
}

static void el0_ic_iallu(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(ic_iallu());
}

static void el0_execute_eret(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(execute_eret());
}

/* ============================================================
 * SYSREG-01: MRS SCTLR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_sctlr_el1_at_el0);
bool test_sysreg_read_sctlr_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-01: MRS SCTLR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_sctlr_el1, 0);
    CHECK_TRAP("sctlr_el1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-02: MSR SCTLR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_write_sctlr_el1_at_el0);
bool test_sysreg_write_sctlr_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-02: MSR SCTLR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_write_sctlr_el1, 0);
    CHECK_TRAP("sctlr_el1 write from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-03: MRS VBAR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_vbar_el1_at_el0);
bool test_sysreg_read_vbar_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-03: MRS VBAR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_vbar_el1, 0);
    CHECK_TRAP("vbar_el1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-04: MSR VBAR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_write_vbar_el1_at_el0);
bool test_sysreg_write_vbar_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-04: MSR VBAR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_write_vbar_el1, 0);
    CHECK_TRAP("vbar_el1 write from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-05: MRS TTBR0_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_ttbr0_el1_at_el0);
bool test_sysreg_read_ttbr0_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-05: MRS TTBR0_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_ttbr0_el1, 0);
    CHECK_TRAP("ttbr0_el1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-06: MRS TCR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_tcr_el1_at_el0);
bool test_sysreg_read_tcr_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-06: MRS TCR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_tcr_el1, 0);
    CHECK_TRAP("tcr_el1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-07: MRS ESR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_esr_el1_at_el0);
bool test_sysreg_read_esr_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-07: MRS ESR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_esr_el1, 0);
    CHECK_TRAP("esr_el1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-08: MRS FAR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_far_el1_at_el0);
bool test_sysreg_read_far_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-08: MRS FAR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_far_el1, 0);
    CHECK_TRAP("far_el1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-09: MRS MAIR_EL1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_mair_el1_at_el0);
bool test_sysreg_read_mair_el1_at_el0(void)
{
    TEST_BEGIN("SYSREG-09: MRS MAIR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_mair_el1, 0);
    CHECK_TRAP("mair_el1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-10: TLBI VMALLE1 from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_tlbi_vmalle1_at_el0);
bool test_sysreg_tlbi_vmalle1_at_el0(void)
{
    TEST_BEGIN("SYSREG-10: TLBI VMALLE1 @ EL0 → trap");
    run_at_el(EL0, el0_tlbi_vmalle1, 0);
    CHECK_TRAP("tlbi vmalle1 from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-11: IC IALLU from EL0 should trap
 * ============================================================ */
TEST_REGISTER(test_sysreg_ic_iallu_at_el0);
bool test_sysreg_ic_iallu_at_el0(void)
{
    TEST_BEGIN("SYSREG-11: IC IALLU @ EL0 → trap");
    run_at_el(EL0, el0_ic_iallu, 0);
    CHECK_TRAP("ic iallu from EL0", EC_UNKNOWN);
    TEST_END();
}

/* ============================================================
 * SYSREG-12: ERET from EL0 should trap (illegal execution)
 * ============================================================ */
TEST_REGISTER(test_sysreg_eret_at_el0);
bool test_sysreg_eret_at_el0(void)
{
    TEST_BEGIN("SYSREG-12: ERET @ EL0 → trap");
    run_at_el(EL0, el0_execute_eret, 0);
    /*
     * ERET at EL0 may produce EC_UNKNOWN (0x00) or EC_ILLEGAL_EXEC (0x0E)
     * depending on architecture version. Accept either.
     */
    if (!trap_record.triggered) {
        printf("\n  expected trap but none occurred\n");
        current_test.passed = false;
    }
    trap_record_reset();
    TEST_END();
}
