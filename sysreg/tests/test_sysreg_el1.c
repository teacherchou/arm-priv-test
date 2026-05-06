/*
 * System Register Access Privilege Tests - EL1 Positive Verification
 *
 * Verifies that accessing EL1 system registers from EL1 succeeds
 * without triggering any exception.
 */
#include "test_framework.h"
#include "sysreg_ops.h"

static volatile uint64_t sysreg_el1_sink;

/* ============================================================
 * SYSREG-E1-01: MRS SCTLR_EL1 from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_sctlr_el1_at_el1);
bool test_sysreg_read_sctlr_el1_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-01: MRS SCTLR_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(sysreg_el1_sink = read_sctlr_el1());
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-02: MRS/MSR VBAR_EL1 from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_rw_vbar_el1_at_el1);
bool test_sysreg_rw_vbar_el1_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-02: MRS/MSR VBAR_EL1 @ EL1 → ok");
    uint64_t original = read_vbar_el1();
    EXPECT_NO_TRAP(write_vbar_el1(original));
    EXPECT_NO_TRAP(sysreg_el1_sink = read_vbar_el1());
    TEST_ASSERT_EQ("vbar_el1 roundtrip", sysreg_el1_sink, original);
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-03: MRS TTBR0_EL1 from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_ttbr0_el1_at_el1);
bool test_sysreg_read_ttbr0_el1_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-03: MRS TTBR0_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(sysreg_el1_sink = read_ttbr0_el1());
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-04: MRS TCR_EL1 from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_tcr_el1_at_el1);
bool test_sysreg_read_tcr_el1_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-04: MRS TCR_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(sysreg_el1_sink = read_tcr_el1());
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-05: MRS MAIR_EL1 from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_mair_el1_at_el1);
bool test_sysreg_read_mair_el1_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-05: MRS MAIR_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(sysreg_el1_sink = read_mair_el1());
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-06: TLBI VMALLE1 from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_tlbi_vmalle1_at_el1);
bool test_sysreg_tlbi_vmalle1_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-06: TLBI VMALLE1 @ EL1 → ok");
    EXPECT_NO_TRAP(tlbi_vmalle1());
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-07: IC IALLU from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_ic_iallu_at_el1);
bool test_sysreg_ic_iallu_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-07: IC IALLU @ EL1 → ok");
    EXPECT_NO_TRAP(ic_iallu());
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-08: DAIF read from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_read_daif_at_el1);
bool test_sysreg_read_daif_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-08: MRS DAIF @ EL1 → ok");
    EXPECT_NO_TRAP(sysreg_el1_sink = read_daif());
    TEST_END();
}

/* ============================================================
 * SYSREG-E1-09: EL0-accessible TPIDR_EL0 from EL1 should succeed
 * ============================================================ */
TEST_REGISTER(test_sysreg_rw_tpidr_el0_at_el1);
bool test_sysreg_rw_tpidr_el0_at_el1(void)
{
    TEST_BEGIN("SYSREG-E1-09: MRS/MSR TPIDR_EL0 @ EL1 → ok");
    uint64_t test_val = 0xDEADBEEFCAFE0001ULL;
    EXPECT_NO_TRAP(write_tpidr_el0(test_val));
    EXPECT_NO_TRAP(sysreg_el1_sink = read_tpidr_el0());
    TEST_ASSERT_EQ("tpidr_el0 roundtrip", sysreg_el1_sink, test_val);
    TEST_END();
}
